#ifndef GATEWAY_H
#define GATEWAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "protocol.h"

/* 前向声明: 原项目的线程池类型 (可选依赖) */
struct Threadpool;

/*
 * 智能网关通信模块
 *
 * 功能:
 *   1. TCP 服务器监听 (替代粗糙的回显服务器)
 *   2. 多客户端并发连接管理 (STM32 + ESP8266 设备)
 *   3. 协议帧解析与发送 (基于 protocol.h)
 *   4. 心跳保活机制
 *   5. 设备注册与状态追踪
 *
 * 架构:
 *   accept 线程 -> 接受连接 -> 创建设备记录 -> 投递到线程池
 *   client_worker -> 从线程池获取 -> 读取/解析帧 -> 分发到回调
 *   heartbeat 线程 -> 定期检测设备活跃度 -> 超时踢除
 *
 * 用法 (阻塞模式):
 *   gateway_init(port)
 *   gateway_register_handler(CMD_SENSOR_DATA, my_handler)
 *   gateway_run()      // 阻塞运行
 *   gateway_stop()
 *   gateway_cleanup()
 *
 * 用法 (交互模式，集成线程池):
 *   gateway_init(port)
 *   gateway_register_handler(...)
 *   gateway_run_interactive(ctx, pool)   // 阻塞直到用户按 q
 *   gateway_cleanup()
 */

#define GATEWAY_DEFAULT_PORT    6789
#define GATEWAY_MAX_CLIENTS     32
#define GATEWAY_HEARTBEAT_TIMEOUT_S  30   /* 心跳超时时间 */
#define GATEWAY_HEARTBEAT_INTERVAL_S 10  /* 心跳检测间隔 */

/* 设备类型 */
#define DEV_TYPE_STM32      0x01
#define DEV_TYPE_ESP8266    0x02
#define DEV_TYPE_UNKNOWN    0xFF

/* 设备信息 */
typedef struct {
    int fd;
    char ip[32];
    int port;
    uint8_t dev_type;
    char dev_id[32];       /* 设备唯一标识 */
    uint16_t last_seq;     /* 最新序列号 */
    int heartbeat_fail;    /* 连续心跳失败次数 */
    long last_active_time; /* 最后活跃时间 (time_t) */
} GatewayDevice;

/* 消息回调函数签名 */
typedef void (*GatewayHandler)(const GatewayDevice *dev,
                               uint8_t cmd, uint16_t seq,
                               const uint8_t *payload, uint16_t payload_len,
                               void *user_data);

/* 网关上下文 (不透明类型) */
typedef struct GatewayCtx GatewayCtx;

/**
 * @brief 初始化网关服务器
 * @param port 监听端口
 * @return 网关上下文指针, NULL 失败
 */
GatewayCtx *gateway_init(uint16_t port);

/**
 * @brief 注册消息处理回调
 * @param ctx 网关上下文
 * @param cmd 命令字
 * @param handler 回调函数
 * @param user_data 用户数据 (传给回调)
 */
void gateway_register_handler(GatewayCtx *ctx, uint8_t cmd,
                              GatewayHandler handler, void *user_data);

/**
 * @brief 运行网关 (阻塞，直到 gateway_stop 被调用)
 * @param ctx 网关上下文
 */
void gateway_run(GatewayCtx *ctx);

/**
 * @brief 运行网关并启动交互式命令界面 (集成线程池)
 *        在 gateway> 提示符下接受 q/s/h/l/r 命令
 *        此函数阻塞直到用户按 'q' 退出
 * @param ctx 网关上下文
 * @param pool 线程池指针 (可为 NULL，退化为独立线程模式)
 */
void gateway_run_interactive(GatewayCtx *ctx, struct Threadpool *pool);

/**
 * @brief 停止网关服务器
 * @param ctx 网关上下文
 */
void gateway_stop(GatewayCtx *ctx);

/**
 * @brief 清理网关资源
 * @param ctx 网关上下文
 */
void gateway_cleanup(GatewayCtx *ctx);

/**
 * @brief 向指定设备发送响应帧
 * @param ctx 网关上下文
 * @param dev_fd 设备 fd
 * @param cmd 命令字
 * @param flags 标志位
 * @param seq 序列号
 * @param payload 载荷
 * @param payload_len 载荷长度
 * @return 发送字节数, -1 失败
 */
int gateway_send_response(GatewayCtx *ctx, int dev_fd, uint8_t cmd, uint8_t flags,
                          uint16_t seq, const uint8_t *payload, uint16_t payload_len);

/**
 * @brief 向指定设备发送请求帧
 */
static inline int gateway_send_request(GatewayCtx *ctx, int dev_fd, uint8_t cmd,
                                       uint16_t seq, const uint8_t *payload, uint16_t payload_len) {
    return gateway_send_response(ctx, dev_fd, cmd, FLAG_REQUEST, seq, payload, payload_len);
}

/**
 * @brief 获取已注册设备列表
 * @param ctx 网关上下文
 * @param count 输出设备数量
 * @return 设备数组指针 (内部数据，勿修改)
 */
const GatewayDevice *gateway_get_devices(GatewayCtx *ctx, int *count);

/**
 * @brief 根据 fd 查找设备
 */
const GatewayDevice *gateway_find_device(GatewayCtx *ctx, int fd);

/**
 * @brief 打印当前连接设备状态
 */
void gateway_print_status(GatewayCtx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_H */
