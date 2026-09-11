#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>

/* ============ 协议常量 ============ */
#define PROTO_MAGIC_1       0xAA
#define PROTO_MAGIC_2       0x55
#define PROTO_HEADER_SIZE   8
#define PROTO_CHECKSUM_SIZE 1

#define CMD_PING            0x01
#define CMD_STM32_REGISTER  0x10
#define CMD_SENSOR_DATA     0x11
#define CMD_GPIO_CONTROL    0x20

#define FLAG_REQUEST        0x00
#define FLAG_RESPONSE       0x01
#define FLAG_HEARTBEAT      0x02

/* ============ 全局状态 ============ */
static int      g_sockfd       = -1;
static uint16_t g_next_seq     = 1;
static int      g_sent_count   = 0;
static int      g_recv_count   = 0;
static time_t   g_connect_time = 0;
static time_t   g_last_recv    = 0;
static const char *g_server_ip   = "";
static int      g_server_port    = 0;
static int      g_verbose        = 0;   /* 心跳日志开关，默认静默 */
static int      g_auto_mode      = 0;   /* 自动模式（后台运行时） */
static int      g_client_id      = 0;   /* 客户端编号 */

/* ============ 异或校验 ============ */
static uint8_t checksum(const uint8_t *data, int len)
{
    uint8_t cs = 0;
    for (int i = 0; i < len; i++) cs ^= data[i];
    return cs;
}

/* ============ 构造帧 ============ */
static int build_frame(uint8_t *out, uint8_t cmd, uint8_t flags,
                       uint16_t seq, const uint8_t *payload, uint16_t plen)
{
    int i = 0;
    out[i++] = PROTO_MAGIC_1;
    out[i++] = PROTO_MAGIC_2;
    out[i++] = cmd;
    out[i++] = flags;
    out[i++] = seq & 0xFF;
    out[i++] = (seq >> 8) & 0xFF;
    out[i++] = plen & 0xFF;
    out[i++] = (plen >> 8) & 0xFF;
    if (payload && plen > 0) {
        memcpy(out + i, payload, plen);
        i += plen;
    }
    out[i] = checksum(out, i);
    i += 1;
    return i;
}

/* ============ 发送帧并计数 ============ */
static int send_frame(uint8_t cmd, uint8_t flags, uint16_t seq,
                      const uint8_t *payload, uint16_t plen)
{
    uint8_t frame[2048];
    int len = build_frame(frame, cmd, flags, seq, payload, plen);
    int sent = send(g_sockfd, frame, len, 0);
    if (sent > 0) g_sent_count++;
    return sent;
}

/* ============ 显示帮助 ============ */
static void show_help(void)
{
    printf("\n╔════════════════════════════════════════════════╗\n");
    printf("║           Client Commands (交互命令)          ║\n");
    printf("╠════════════════════════════════════════════════╣\n");
    printf("║  h  - Show this help                          ║\n");
    printf("║  s  - Show connection status                  ║\n");
    printf("║  q  - Quit client                             ║\n");
    printf("║  r  - Send register request   (CMD=0x10)      ║\n");
    printf("║  d  - Send sensor data        (CMD=0x11)      ║\n");
    printf("║  g  - Send GPIO control       (CMD=0x20)      ║\n");
    printf("║  p  - Send PING               (CMD=0x01)      ║\n");
    printf("║  v  - Toggle heartbeat log    (当前: %-3s)   ║\n",
           g_verbose ? "ON" : "OFF");
    printf("╚════════════════════════════════════════════════╝\n\n");
}

/* ============ 显示状态 ============ */
static void show_status(void)
{
    time_t now = time(NULL);
    printf("\n════════════ Client Status ════════════\n");
    printf("  Client ID    : %d\n", g_client_id);
    printf("  Server       : %s:%d\n", g_server_ip, g_server_port);
    printf("  Connected    : %ld s\n", (long)(now - g_connect_time));
    printf("  Last recv    : %ld s ago\n", (long)(now - g_last_recv));
    printf("  Frames sent  : %d\n", g_sent_count);
    printf("  Frames recv  : %d\n", g_recv_count);
    printf("  Next seq     : %u\n", g_next_seq);
    printf("  Mode         : %s\n", g_auto_mode ? "AUTO" : "INTERACTIVE");
    printf("  Heartbeat log: %s\n", g_verbose ? "ON" : "OFF");
    printf("═══════════════════════════════════════\n\n");
}

/* ============ 处理服务器发来的数据 ============
 * 返回值：本次是否打印了内容（用于决定是否重绘提示符）
 * ============================================================ */
static int handle_server_data(const uint8_t *buf, int len)
{
    int offset = 0;
    int printed = 0;

    while (offset + PROTO_HEADER_SIZE <= len) {
        /* 校验魔数 */
        if (buf[offset] != PROTO_MAGIC_1 || buf[offset+1] != PROTO_MAGIC_2) {
            offset++;
            continue;
        }

        uint16_t plen = buf[offset+6] | (buf[offset+7] << 8);
        int frame_len = PROTO_HEADER_SIZE + plen + PROTO_CHECKSUM_SIZE;

        /* 半包：等下次 recv */
        if (offset + frame_len > len) break;

        uint8_t  cmd   = buf[offset+2];
        uint8_t  flags = buf[offset+3];
        uint16_t seq   = buf[offset+4] | (buf[offset+5] << 8);

        /* PING → 自动回 PONG（心跳，按需打印） */
        if (cmd == CMD_PING && (flags & FLAG_HEARTBEAT)) {
            send_frame(CMD_PING, FLAG_HEARTBEAT, seq, NULL, 0);
            if (g_verbose && !g_auto_mode) {
                printf("\n💓 [心跳] 收到 PING seq=%u，已回 PONG\n", seq);
                printed = 1;
            }
            g_recv_count++;
            offset += frame_len;
            continue;
        }

        /* 其他帧：仅交互模式打印 */
        if (!g_auto_mode) {
            printf("\n📥 [服务器] cmd=0x%02X flags=0x%02X seq=%u payload_len=%u\n",
                   cmd, flags, seq, plen);

            if (plen > 0) {
                printf("   payload: ");
                for (int i = 0; i < plen && i < 16; i++)
                    printf("%02X ", buf[offset + PROTO_HEADER_SIZE + i]);
                if (plen > 16) printf("...");
                printf("\n");
            }
            printed = 1;
        }

        g_recv_count++;
        offset += frame_len;
    }

    return printed;
}

/* ============ 打印提示符 ============ */
static void print_prompt(void)
{
    printf("client> ");
    fflush(stdout);
}

/* ============ 注册 ============ */
static void do_register(void)
{
    char id_buf[32];
    if (g_client_id > 0)
        snprintf(id_buf, sizeof(id_buf), "CLIENT-%02d", g_client_id);
    else
        snprintf(id_buf, sizeof(id_buf), "LINUX-CLIENT");

    send_frame(CMD_STM32_REGISTER, FLAG_REQUEST, g_next_seq++,
               (uint8_t*)id_buf, strlen(id_buf));

    if (!g_auto_mode)
        printf("📤 [注册请求] seq=%u id=%s\n", g_next_seq - 1, id_buf);
}

/* ============ 传感器数据 ============ */
static void do_sensor(void)
{
    uint8_t payload[4] = {0x01, 0x02, 0x03, 0x04};
    send_frame(CMD_SENSOR_DATA, FLAG_REQUEST, g_next_seq++, payload, 4);
    if (!g_auto_mode)
        printf("📤 [传感器数据] seq=%u payload=01 02 03 04\n", g_next_seq - 1);
}

/* ============ GPIO 控制 ============ */
static void do_gpio(void)
{
    uint8_t payload[2] = {0x07, 0x01};   /* PA7 = HIGH */
    send_frame(CMD_GPIO_CONTROL, FLAG_REQUEST, g_next_seq++, payload, 2);
    if (!g_auto_mode)
        printf("📤 [GPIO 控制] seq=%u pin=7 val=1\n", g_next_seq - 1);
}

/* ============ PING ============ */
static void do_ping(void)
{
    send_frame(CMD_PING, FLAG_HEARTBEAT, g_next_seq++, NULL, 0);
    if (!g_auto_mode)
        printf("📤 [PING] seq=%u\n", g_next_seq - 1);
}

/* ============ main ============ */
int main(int argc, char *argv[])
{
    /* 扫描 -a 和 -i 标志，同时收集位置参数 */
    const char *pos_args[2] = {NULL, NULL};
    int pos_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            g_auto_mode = 1;
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            g_client_id = atoi(argv[++i]);
        } else if (pos_count < 2) {
            pos_args[pos_count++] = argv[i];
        }
    }

    g_server_ip   = (pos_count >= 1) ? pos_args[0] : "127.0.0.1";
    g_server_port = (pos_count >= 2) ? atoi(pos_args[1]) : 6789;

    /* 1. 创建 socket */
    g_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_sockfd < 0) { perror("socket"); return -1; }

    /* 2. 连接 */
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(g_server_port);
    inet_pton(AF_INET, g_server_ip, &addr.sin_addr);

    if (connect(g_sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(g_sockfd);
        return -1;
    }

    g_connect_time = time(NULL);
    g_last_recv    = g_connect_time;

    if (!g_auto_mode) {
        printf("✅ 已连接 %s:%d\n", g_server_ip, g_server_port);
    }

    /* 3. 自动发送一次注册 */
    do_register();

    if (g_auto_mode) {
        /* 自动模式：静默运行，不打印 */
    } else {
        printf("💡 输入 h 查看帮助，q 退出\n");
        printf("💡 心跳日志默认关闭，按 v 可切换显示\n\n");
        print_prompt();
    }

    /* 4. 主循环 */
    int running = 1;
    time_t last_auto_send = time(NULL);

    while (running) {
        /* =============================================
         * 关键：自动发送放在循环开头
         * 不依赖 select 超时（服务器每秒 PING，select 永不超时）
         * ============================================= */
        if (g_auto_mode) {
            time_t now = time(NULL);
            if (now - last_auto_send >= 3) {
                last_auto_send = now;
                static int counter = 0;
                counter = (counter + 1) % 3;
                if (counter == 0)      do_sensor();
                else if (counter == 1) do_gpio();
                else                   do_ping();
            }
        }

        fd_set rfds;
        struct timeval tv = {1, 0};

        FD_ZERO(&rfds);
        FD_SET(g_sockfd, &rfds);
        int maxfd = g_sockfd;

        /* 交互模式才监听键盘 */
        if (!g_auto_mode) {
            FD_SET(STDIN_FILENO, &rfds);
            if (STDIN_FILENO > maxfd) maxfd = STDIN_FILENO;
        }

        int ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }
        if (ret == 0) continue;

        /* ---- socket 有数据 ---- */
        if (FD_ISSET(g_sockfd, &rfds)) {
            uint8_t buf[2048];
            int n = recv(g_sockfd, buf, sizeof(buf), 0);
            if (n > 0) {
                g_last_recv = time(NULL);
                int printed = handle_server_data(buf, n);
                if (printed) print_prompt();
            } else if (n == 0) {
                if (!g_auto_mode) printf("\n❌ 服务器已断开连接\n");
                break;
            } else {
                if (errno == EINTR) continue;
                perror("recv");
                break;
            }
        }

        /* ---- stdin 有输入（仅交互模式） ---- */
        if (!g_auto_mode && FD_ISSET(STDIN_FILENO, &rfds)) {
            char line[256];
            if (fgets(line, sizeof(line), stdin) == NULL) break;
            line[strcspn(line, "\n")] = 0;

            if (line[0] == '\0') {
                print_prompt();
                continue;
            }

            switch (line[0]) {
                case 'h': show_help(); break;
                case 's': show_status(); break;
                case 'q':
                    printf("👋 正在退出...\n");
                    running = 0;
                    break;
                case 'r': do_register(); break;
                case 'd': do_sensor(); break;
                case 'g': do_gpio(); break;
                case 'p': do_ping(); break;
                case 'v':
                    g_verbose = !g_verbose;
                    printf("💓 心跳日志已 %s\n", g_verbose ? "开启" : "关闭");
                    break;
                default:
                    printf("❓ 未知命令 '%c'，输入 h 查看帮助\n", line[0]);
                    break;
            }

            if (running) print_prompt();
        }
    }

    close(g_sockfd);
    if (!g_auto_mode) printf("👋 客户端退出\n");
    return 0;
}