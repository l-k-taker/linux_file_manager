#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>
#include <stdint.h>
#include "gateway.h"
#include "protocol.h"
#include "thread_pool.h"

/*
 * 网关通信模块实现
 *
 * 架构:
 *   accept 线程  -> 接受连接 -> 创建设备记录 -> 启动 client_worker 线程
 *   client_worker -> 循环读取/解析协议帧 -> 分发到注册的 handler 回调
 *   heartbeat 线程 -> 定期检测设备活跃度 -> 超时踢除
 */

/* 传递给工作线程的参数 */
typedef struct {
    GatewayCtx *ctx;
    int fd;
} ClientWorkerArg;

/* 消息处理节点 */
struct HandlerNode {
    uint8_t cmd;
    GatewayHandler handler;
    void *user_data;
    struct HandlerNode *next;
};

/* 网关上下文 */
struct GatewayCtx {
    uint16_t port;
    int listen_fd;
    volatile int running;
    volatile int stopped;   /* 防止重复清理 */

    GatewayDevice devices[GATEWAY_MAX_CLIENTS];
    int device_count;
    pthread_mutex_t devices_mutex;

    struct HandlerNode *handlers;
    pthread_mutex_t handlers_mutex;

    struct Threadpool *pool;

    pthread_t accept_thread_id;
    pthread_t heartbeat_thread_id;
};

/* 内部函数 */
static void *accept_thread_func(void *arg);
static void *heartbeat_thread_func(void *arg);
static void *client_worker(void *arg);
static void client_worker_wrapper(void *arg);
static int send_frame_raw(GatewayCtx *ctx, int fd, uint8_t cmd, uint8_t flags,
                          uint16_t seq, const uint8_t *payload, uint16_t payload_len);
static int add_device(GatewayCtx *ctx, int fd, const char *ip, int port);
static void remove_device(GatewayCtx *ctx, int fd);
static void update_device_activity(GatewayCtx *ctx, int fd);
static const GatewayDevice *find_device_by_fd(GatewayCtx *ctx, int fd);
static struct HandlerNode *find_handler(GatewayCtx *ctx, uint8_t cmd);
static void *client_worker_detached(void *arg);

/* 交互式菜单线程参数 */
typedef struct {
    GatewayCtx *ctx;
    struct Threadpool *pool;
} InteractiveArg;

/* 交互式命令线程 */
static void *interactive_thread_func(void *arg);

/* ==================== 公共接口 ==================== */

GatewayCtx *gateway_init(uint16_t port) {
    GatewayCtx *ctx = (GatewayCtx *)calloc(1, sizeof(GatewayCtx));
    if (!ctx) {
        perror("[GW] calloc");
        return NULL;
    }

    ctx->port = port;
    ctx->listen_fd = -1;
    ctx->running = 0;
    ctx->device_count = 0;
    ctx->handlers = NULL;
    ctx->pool = NULL;

    pthread_mutex_init(&ctx->devices_mutex, NULL);
    pthread_mutex_init(&ctx->handlers_mutex, NULL);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("[GW] socket");
        free(ctx);
        return NULL;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[GW] bind");
        close(fd);
        free(ctx);
        return NULL;
    }

    if (listen(fd, 16) < 0) {
        perror("[GW] listen");
        close(fd);
        free(ctx);
        return NULL;
    }

    ctx->listen_fd = fd;
    printf("[GW] Gateway initialized on port %d\n", port);
    return ctx;
}

void gateway_register_handler(GatewayCtx *ctx, uint8_t cmd,
                              GatewayHandler handler, void *user_data) {
    if (!ctx || !handler) return;

    struct HandlerNode *node = (struct HandlerNode *)calloc(1, sizeof(struct HandlerNode));
    node->cmd = cmd;
    node->handler = handler;
    node->user_data = user_data;

    pthread_mutex_lock(&ctx->handlers_mutex);
    node->next = ctx->handlers;
    ctx->handlers = node;
    pthread_mutex_unlock(&ctx->handlers_mutex);

    printf("[GW] Handler registered for CMD 0x%02X\n", cmd);
}

void gateway_run(GatewayCtx *ctx) {
    if (!ctx || ctx->listen_fd < 0) return;
    ctx->running = 1;

    if (pthread_create(&ctx->accept_thread_id, NULL, accept_thread_func, ctx) != 0) {
        perror("[GW] pthread_create accept");
        return;
    }

    if (pthread_create(&ctx->heartbeat_thread_id, NULL, heartbeat_thread_func, ctx) != 0) {
        perror("[GW] pthread_create heartbeat");
        ctx->running = 0;
        return;
    }

    printf("[GW] Gateway running on port %d\n", ctx->port);

    while (ctx->running) {
        sleep(1);
    }
    printf("[GW] Gateway main loop exited\n");
}

/**
 * @brief 运行网关并启动交互式命令界面 (集成线程池)
 * 在 gateway> 提示符下接受 q/s/h/l/r 命令
 */
void gateway_run_interactive(GatewayCtx *ctx, struct Threadpool *pool) {
    if (!ctx || ctx->listen_fd < 0) return;

    ctx->pool = pool;
    ctx->running = 1;
    ctx->accept_thread_id = 0;
    ctx->heartbeat_thread_id = 0;

    /* 启动 accept 线程 */
    if (pthread_create(&ctx->accept_thread_id, NULL, accept_thread_func, ctx) != 0) {
        perror("[GW] pthread_create accept");
        ctx->accept_thread_id = 0;
        ctx->running = 0;
        return;
    }

    /* 启动心跳线程 */
    if (pthread_create(&ctx->heartbeat_thread_id, NULL, heartbeat_thread_func, ctx) != 0) {
        perror("[GW] pthread_create heartbeat");
        ctx->heartbeat_thread_id = 0;
        ctx->running = 0;
        /* 等待 accept 线程退出，然后关闭 listen_fd */
        if (ctx->listen_fd > 0) {
            close(ctx->listen_fd);
            ctx->listen_fd = -1;
        }
        if (ctx->accept_thread_id) {
            pthread_join(ctx->accept_thread_id, NULL);
            ctx->accept_thread_id = 0;
        }
        return;
    }

    printf("\n[GW] Gateway server is running on port %d\n", ctx->port);
    printf("[GW] Type 'h' for help, 'q' to quit, 's' for status\n\n");

    /* 启动交互式命令线程 */
    InteractiveArg iarg;
    iarg.ctx = ctx;
    iarg.pool = pool;
    pthread_t interactive_tid;
    if (pthread_create(&interactive_tid, NULL, interactive_thread_func, &iarg) != 0) {
        perror("[GW] pthread_create interactive");
        ctx->running = 0;
        /* 等待 accept 和 heartbeat 线程退出 */
        if (ctx->listen_fd > 0) {
            close(ctx->listen_fd);
            ctx->listen_fd = -1;
        }
        if (ctx->accept_thread_id) {
            pthread_join(ctx->accept_thread_id, NULL);
            ctx->accept_thread_id = 0;
        }
        if (ctx->heartbeat_thread_id) {
            pthread_join(ctx->heartbeat_thread_id, NULL);
            ctx->heartbeat_thread_id = 0;
        }
        return;
    }

    /* 等待交互式命令线程退出 (用户按了 q) */
    pthread_join(interactive_tid, NULL);
    printf("[GW] Interactive command thread exited\n");
}

void gateway_stop(GatewayCtx *ctx) {
    if (!ctx) return;
    if (ctx->stopped) return;
    ctx->stopped = 1;
    ctx->running = 0;

    /*
     * 1. 关闭 listen_fd 以唤醒阻塞在 accept() 的线程
     *    注意: shutdown() 在 Linux 上不会唤醒 accept(), 必须用 close()
     */
    if (ctx->listen_fd > 0) {
        close(ctx->listen_fd);
        ctx->listen_fd = -1;
    }

    /* 2. 等待 accept 线程退出 */
    if (ctx->accept_thread_id) {
        pthread_join(ctx->accept_thread_id, NULL);
        ctx->accept_thread_id = 0;
    }

    /* 3. 等待心跳线程退出 */
    if (ctx->heartbeat_thread_id) {
        pthread_join(ctx->heartbeat_thread_id, NULL);
        ctx->heartbeat_thread_id = 0;
    }

    /* 4. 关闭所有客户端连接 */
    pthread_mutex_lock(&ctx->devices_mutex);
    for (int i = 0; i < ctx->device_count; i++) {
        if (ctx->devices[i].fd > 0) {
            shutdown(ctx->devices[i].fd, SHUT_RDWR);
            close(ctx->devices[i].fd);
        }
    }
    ctx->device_count = 0;
    pthread_mutex_unlock(&ctx->devices_mutex);
}

void gateway_cleanup(GatewayCtx *ctx) {
    if (!ctx) return;

    pthread_mutex_lock(&ctx->handlers_mutex);
    struct HandlerNode *cur = ctx->handlers;
    while (cur) {
        struct HandlerNode *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    ctx->handlers = NULL;
    pthread_mutex_unlock(&ctx->handlers_mutex);

    pthread_mutex_destroy(&ctx->devices_mutex);
    pthread_mutex_destroy(&ctx->handlers_mutex);
    free(ctx);
}

int gateway_send_response(GatewayCtx *ctx, int dev_fd, uint8_t cmd, uint8_t flags,
                          uint16_t seq, const uint8_t *payload, uint16_t payload_len) {
    if (!ctx || dev_fd < 0) return -1;
    return send_frame_raw(ctx, dev_fd, cmd, flags, seq, payload, payload_len);
}

const GatewayDevice *gateway_get_devices(GatewayCtx *ctx, int *count) {
    if (!ctx || !count) return NULL;
    pthread_mutex_lock(&ctx->devices_mutex);
    *count = ctx->device_count;
    const GatewayDevice *ptr = ctx->devices;
    pthread_mutex_unlock(&ctx->devices_mutex);
    return ptr;
}

const GatewayDevice *gateway_find_device(GatewayCtx *ctx, int fd) {
    return find_device_by_fd(ctx, fd);
}

void gateway_print_status(GatewayCtx *ctx) {
    if (!ctx) return;
    pthread_mutex_lock(&ctx->devices_mutex);
    printf("\n╔═══════════════════════════════════════════════════════╗\n");
    printf("║              GATEWAY DEVICE STATUS                   ║\n");
    printf("╠═══════════════════════════════════════════════════════╣\n");
    printf("║  Port: %-4d                                      ║\n", ctx->port);
    printf("║  Devices: %d/%d                                  ║\n", ctx->device_count, GATEWAY_MAX_CLIENTS);
    printf("╠═══════════════════════════════════════════════════════╣\n");
    if (ctx->device_count == 0) {
        printf("║  (No devices connected)                            ║\n");
    } else {
        for (int i = 0; i < ctx->device_count; i++) {
            const GatewayDevice *d = &ctx->devices[i];
            const char *type_str = "UNKNOWN";
            if (d->dev_type == DEV_TYPE_STM32) type_str = "STM32";
            else if (d->dev_type == DEV_TYPE_ESP8266) type_str = "ESP8266";
            printf("║  [%d] %s:%-5d %-8s ID=%-16s ║\n",
                   i + 1, d->ip, d->port, type_str, d->dev_id);
        }
    }
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    pthread_mutex_unlock(&ctx->devices_mutex);
}

/* ==================== 内部实现 ==================== */

/* Accept 线程 */
static void *accept_thread_func(void *arg) {
    GatewayCtx *ctx = (GatewayCtx *)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (ctx->running) {
        /* 使用 select() 超时替代阻塞 accept()，以便周期性检查 ctx->running */
        fd_set read_fds;
        struct timeval tv;
        FD_ZERO(&read_fds);
        FD_SET(ctx->listen_fd, &read_fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(ctx->listen_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (!ctx->running) break;
            if (errno == EINTR) continue;
            perror("[GW] select");
            break;
        }
        if (ret == 0) continue; /* 超时，重新检查 ctx->running */

        int client_fd = accept(ctx->listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (!ctx->running) break;
            if (errno == EINTR) continue;
            perror("[GW] accept");
            continue;
        }

        char ip[32];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        int port = ntohs(client_addr.sin_port);

        printf("[GW] + Client %s:%d connected\n", ip, port);

        if (add_device(ctx, client_fd, ip, port) < 0) {
            close(client_fd);
            continue;
        }

        /* 创建 worker 参数 */
        ClientWorkerArg *warg = (ClientWorkerArg *)malloc(sizeof(ClientWorkerArg));
        warg->ctx = ctx;
        warg->fd = client_fd;

        /* 如果线程池可用，投递到线程池；否则创建独立线程 */
        if (ctx->pool) {
            threadPoolAdd(ctx->pool, client_worker_wrapper, warg);
        } else {
            pthread_t tid;
            pthread_create(&tid, NULL, client_worker_detached, warg);
            pthread_detach(tid);
        }
    }

    return NULL;
}

/* 心跳检测线程 */
static void *heartbeat_thread_func(void *arg) {
    GatewayCtx *ctx = (GatewayCtx *)arg;
    time_t last_check = time(NULL);

    while (ctx->running) {
        sleep(1);
        time_t now = time(NULL);
        if (now - last_check < GATEWAY_HEARTBEAT_INTERVAL_S) continue;
        last_check = now;

        pthread_mutex_lock(&ctx->devices_mutex);
        for (int i = 0; i < ctx->device_count; ) {
            GatewayDevice *dev = &ctx->devices[i];
            long idle = now - dev->last_active_time;

            if (idle > GATEWAY_HEARTBEAT_TIMEOUT_S) {
                if (dev->heartbeat_fail >= 3) {
                    printf("[GW] ! Device %s:%d heartbeat timeout, disconnecting\n",
                           dev->ip, dev->port);
                    shutdown(dev->fd, SHUT_RDWR);
                    close(dev->fd);
                    for (int j = i; j < ctx->device_count - 1; j++)
                        ctx->devices[j] = ctx->devices[j + 1];
                    ctx->device_count--;
                    continue;
                } else {
                    uint16_t seq = ++dev->last_seq;
                    pthread_mutex_unlock(&ctx->devices_mutex);
                    send_frame_raw(ctx, dev->fd, CMD_PING, FLAG_HEARTBEAT, seq, NULL, 0);
                    pthread_mutex_lock(&ctx->devices_mutex);
                    dev->heartbeat_fail++;
                }
            } else if (idle > GATEWAY_HEARTBEAT_INTERVAL_S) {
                uint16_t seq = ++dev->last_seq;
                pthread_mutex_unlock(&ctx->devices_mutex);
                send_frame_raw(ctx, dev->fd, CMD_PING, FLAG_HEARTBEAT, seq, NULL, 0);
                pthread_mutex_lock(&ctx->devices_mutex);
            } else {
                dev->heartbeat_fail = 0;
            }
            i++;
        }
        pthread_mutex_unlock(&ctx->devices_mutex);
    }
    return NULL;
}

/* 线程池适配 wrapper: void*(void*) -> void(void*) */
static void client_worker_wrapper(void *arg) {
    client_worker(arg);
}

/* 独立线程 wrapper: 自动释放 warg (detach 场景) */
static void *client_worker_detached(void *arg) {
    client_worker(arg);
    free(arg);
    return NULL;
}

/* 客户端工作线程 (pthread 签名) */
static void *client_worker(void *arg) {
    ClientWorkerArg *warg = (ClientWorkerArg *)arg;
    GatewayCtx *ctx = warg->ctx;
    int fd = warg->fd;
    /* 注意: warg 由调用方释放 (线程池在 worker() 中 free(task.arg)) */

    uint8_t recv_buf[PROTO_FRAME_MAX * 4];
    int recv_len = 0;

    printf("[GW] Worker started for fd=%d\n", fd);

    while (ctx->running) {
        fd_set read_fds;
        struct timeval tv;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int ret = select(fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;

        int n = recv(fd, recv_buf + recv_len, sizeof(recv_buf) - recv_len - 1, 0);
        if (n <= 0) {
            break;
        }
        recv_len += n;

        /* 解析帧 */
        int offset = 0;
        while (offset < recv_len) {
            ProtoMessage msg;
            int frame_len = proto_parse_frame(recv_buf + offset, recv_len - offset, &msg);

            if (frame_len > 0) {
                /* 完整帧 */
                const GatewayDevice *dev = find_device_by_fd(ctx, fd);
                if (dev) {
                    update_device_activity(ctx, fd);

                    /* PING -> PONG */
                    if (msg.cmd == CMD_PING && (msg.flags & FLAG_HEARTBEAT)) {
                        send_frame_raw(ctx, fd, CMD_PONG, FLAG_HEARTBEAT, msg.seq, NULL, 0);
                    } else {
                        /* 分发到 handler */
                        struct HandlerNode *node = find_handler(ctx, msg.cmd);
                        if (node) {
                            /* 复制 payload (因为 msg.payload 指向 recv_buf) */
                            uint8_t payload_copy[PROTO_MAX_PAYLOAD];
                            if (msg.payload_len > 0) {
                                memcpy(payload_copy, msg.payload, msg.payload_len);
                            }
                            node->handler(dev, msg.cmd, msg.seq,
                                         msg.payload_len > 0 ? payload_copy : NULL,
                                         msg.payload_len, node->user_data);
                        } else {
                            printf("[GW] No handler for CMD 0x%02X\n", msg.cmd);
                            send_frame_raw(ctx, fd, CMD_ERROR, FLAG_RESPONSE, msg.seq, NULL, 0);
                        }
                    }
                }
                offset += frame_len;
            } else if (frame_len == 0) {
                /* 半包 */
                break;
            } else {
                /* 校验失败，跳过 */
                offset++;
            }
        }

        /* 移动剩余数据 */
        if (offset > 0 && offset < recv_len) {
            memmove(recv_buf, recv_buf + offset, recv_len - offset);
        }
        recv_len -= offset;
    }

    printf("[GW] - Client fd=%d disconnected\n", fd);
    remove_device(ctx, fd);
    shutdown(fd, SHUT_RDWR);
    close(fd);
    return NULL;
}

/* 交互式命令线程 */
static void *interactive_thread_func(void *arg) {
    InteractiveArg *iarg = (InteractiveArg *)arg;
    GatewayCtx *ctx = iarg->ctx;
    struct Threadpool *pool = iarg->pool;

    char cmd_buf[64];
    while (ctx->running) {
        printf("gateway> ");
        if (!fgets(cmd_buf, sizeof(cmd_buf), stdin)) {
            break;
        }

        /* 去除换行符 */
        cmd_buf[strcspn(cmd_buf, "\n")] = 0;
        if (strlen(cmd_buf) == 0) continue;

        switch (cmd_buf[0]) {
            case 'q':
            case 'Q':
                printf("[GW] Stopping gateway server...\n");
                ctx->running = 0;
                break;

            case 's':
            case 'S': {
                int pool_busy = 0, pool_live = 0;
                if (pool) {
                    pool_busy = threadPoolBusyNum(pool);
                    pool_live = threadPoolLiveNum(pool);
                }
                printf("\n===========================================\n");
                printf("        Gateway Server Status\n");
                printf("===========================================\n");
                printf("  Status      : RUNNING\n");
                printf("  Port        : %d\n", ctx->port);
                printf("  Thread Pool : Busy=%d, Live=%d\n", pool_busy, pool_live);
                printf("===========================================\n\n");
                gateway_print_status(ctx);
                break;
            }

            case 'h':
            case 'H':
                printf("\n===========================================\n");
                printf("        Gateway Server Commands\n");
                printf("===========================================\n");
                printf("  q  - Quit gateway server and return to menu\n");
                printf("  s  - Show current server status\n");
                printf("  h  - Show this help message\n");
                printf("  l  - List all connected devices\n");
                printf("  r  - Send restart LED command to all devices\n");
                printf("===========================================\n\n");
                break;

            case 'l':
            case 'L':
                gateway_print_status(ctx);
                break;

            case 'r':
            case 'R': {
                /* Send restart LED command to all devices */
                printf("[GW] Sending restart LED command to all devices...\n");
                pthread_mutex_lock(&ctx->devices_mutex);
                for (int i = 0; i < ctx->device_count; i++) {
                    uint8_t payload[] = {7, 1};  /* PA7=7, HIGH=1 */
                    send_frame_raw(ctx, ctx->devices[i].fd, CMD_GPIO_CONTROL, FLAG_REQUEST,
                                   ctx->devices[i].last_seq + 1, payload, sizeof(payload));
                    printf("  Sent to %s:%d\n", ctx->devices[i].ip, ctx->devices[i].port);
                }
                pthread_mutex_unlock(&ctx->devices_mutex);
                break;
            }

            default:
                printf("[GW] Unknown command: '%s'. Type 'h' for help.\n", cmd_buf);
                break;
        }
    }

    return NULL;
}

/* 发送原始帧 */
static int send_frame_raw(GatewayCtx *ctx, int fd, uint8_t cmd, uint8_t flags,
                          uint16_t seq, const uint8_t *payload, uint16_t payload_len) {
    ProtoFrame frame;
    int frame_len = proto_build_frame(&frame, cmd, flags, seq, payload, payload_len);
    if (frame_len < 0) return -1;

    uint8_t buf[PROTO_FRAME_MAX];
    memcpy(buf, &frame.header, PROTO_HEADER_SIZE);
    if (frame.header.payload_len > 0) {
        memcpy(buf + PROTO_HEADER_SIZE, frame.payload, frame.header.payload_len);
    }
    buf[frame_len - 1] = frame.checksum;

    int sent = send(fd, buf, frame_len, MSG_NOSIGNAL);
    if (sent < 0) {
        perror("[GW] send");
    }
    return sent;
}

static int add_device(GatewayCtx *ctx, int fd, const char *ip, int port) {
    pthread_mutex_lock(&ctx->devices_mutex);
    if (ctx->device_count >= GATEWAY_MAX_CLIENTS) {
        pthread_mutex_unlock(&ctx->devices_mutex);
        fprintf(stderr, "[GW] Device list full\n");
        return -1;
    }
    GatewayDevice *dev = &ctx->devices[ctx->device_count];
    dev->fd = fd;
    strncpy(dev->ip, ip, sizeof(dev->ip) - 1);
    dev->port = port;
    dev->dev_type = DEV_TYPE_UNKNOWN;
    dev->dev_id[0] = '\0';
    dev->last_seq = 0;
    dev->heartbeat_fail = 0;
    dev->last_active_time = time(NULL);
    ctx->device_count++;
    pthread_mutex_unlock(&ctx->devices_mutex);
    return 0;
}

static void remove_device(GatewayCtx *ctx, int fd) {
    pthread_mutex_lock(&ctx->devices_mutex);
    for (int i = 0; i < ctx->device_count; i++) {
        if (ctx->devices[i].fd == fd) {
            for (int j = i; j < ctx->device_count - 1; j++)
                ctx->devices[j] = ctx->devices[j + 1];
            ctx->device_count--;
            break;
        }
    }
    pthread_mutex_unlock(&ctx->devices_mutex);
}

static void update_device_activity(GatewayCtx *ctx, int fd) {
    pthread_mutex_lock(&ctx->devices_mutex);
    for (int i = 0; i < ctx->device_count; i++) {
        if (ctx->devices[i].fd == fd) {
            ctx->devices[i].last_active_time = time(NULL);
            break;
        }
    }
    pthread_mutex_unlock(&ctx->devices_mutex);
}

static const GatewayDevice *find_device_by_fd(GatewayCtx *ctx, int fd) {
    if (!ctx) return NULL;
    pthread_mutex_lock(&ctx->devices_mutex);
    for (int i = 0; i < ctx->device_count; i++) {
        if (ctx->devices[i].fd == fd) {
            const GatewayDevice *dev = &ctx->devices[i];
            pthread_mutex_unlock(&ctx->devices_mutex);
            return dev;
        }
    }
    pthread_mutex_unlock(&ctx->devices_mutex);
    return NULL;
}

static struct HandlerNode *find_handler(GatewayCtx *ctx, uint8_t cmd) {
    pthread_mutex_lock(&ctx->handlers_mutex);
    struct HandlerNode *cur = ctx->handlers;
    while (cur) {
        if (cur->cmd == cmd) {
            pthread_mutex_unlock(&ctx->handlers_mutex);
            return cur;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&ctx->handlers_mutex);
    return NULL;
}
