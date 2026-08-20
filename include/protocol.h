#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * 网关通信协议模块 - 简单二进制帧协议
 * 
 * 帧格式:
 *   [MAGIC(2B)] [CMD(1B)] [FLAGS(1B)] [SEQ(2B)] [PAYLOAD_LEN(2B)] [PAYLOAD(NB)] [CHECKSUM(1B)]
 * 
 *   MAGIC:      0xAA 0x55     - 帧同步魔数
 *   CMD:        命令字        - 标识消息类型
 *   FLAGS:      标志位        - 0x00=请求, 0x01=响应, 0x02=心跳
 *   SEQ:        序列号        - 请求-响应匹配
 *   PAYLOAD_LEN: 载荷长度     - 小端序
 *   PAYLOAD:    载荷数据     - JSON 或二进制
 *   CHECKSUM:   异或校验     - 前面所有字节的异或和
 * 
 * 命令字定义:
 *   0x01 - 心跳 (PING/PONG)
 *   0x10 - STM32 注册
 *   0x11 - 传感器数据上报
 *   0x20 - GPIO 控制命令
 *   0x21 - GPIO 状态查询
 *   0x30 - 固件信息查询
 *   0xF0 - 错误响应
 */

/* 协议常量 */
#define PROTO_MAGIC_1       0xAA
#define PROTO_MAGIC_2       0x55
#define PROTO_HEADER_SIZE   8       /* MAGIC(2) + CMD(1) + FLAGS(1) + SEQ(2) + LEN(2) */
#define PROTO_CHECKSUM_SIZE 1
#define PROTO_MAX_PAYLOAD   512     /* 最大载荷长度 */
#define PROTO_FRAME_MAX     (PROTO_HEADER_SIZE + PROTO_MAX_PAYLOAD + PROTO_CHECKSUM_SIZE)

/* 命令字 */
#define CMD_PING            0x01    /* 心跳请求 */
#define CMD_PONG            0x01    /* 心跳响应 (FLAGS=0x02) */
#define CMD_STM32_REGISTER  0x10    /* STM32 注册请求 */
#define CMD_SENSOR_DATA     0x11    /* 传感器数据上报 */
#define CMD_GPIO_CONTROL    0x20    /* GPIO 控制命令 */
#define CMD_GPIO_QUERY    0x21    /* GPIO 状态查询 */
#define CMD_FW_INFO         0x30    /* 固件信息查询 */
#define CMD_ERROR           0xF0    /* 错误响应 */

/* 标志位 */
#define FLAG_REQUEST        0x00
#define FLAG_RESPONSE       0x01
#define FLAG_HEARTBEAT      0x02

/* 协议帧结构体 (内存布局，非网络字节序) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  magic1;
    uint8_t  magic2;
    uint8_t  cmd;
    uint8_t  flags;
    uint16_t seq;          /* 小端序 */
    uint16_t payload_len;  /* 小端序 */
} ProtoHeader;
#pragma pack(pop)

/* 完整协议帧 */
typedef struct {
    ProtoHeader header;
    uint8_t payload[PROTO_MAX_PAYLOAD];
    uint8_t checksum;
    int total_len;  /* 实际帧长度 */
} ProtoFrame;

/* 解析后的消息 (供业务层使用) */
typedef struct {
    uint8_t  cmd;
    uint8_t  flags;
    uint16_t seq;
    const uint8_t *payload;
    uint16_t payload_len;
} ProtoMessage;

/**
 * @brief 构建完整协议帧
 * @param frame 输出帧缓冲区
 * @param cmd 命令字
 * @param flags 标志位
 * @param seq 序列号
 * @param payload 载荷数据 (可为 NULL)
 * @param payload_len 载荷长度
 * @return 帧总长度, -1 失败
 */
int proto_build_frame(ProtoFrame *frame, uint8_t cmd, uint8_t flags,
                      uint16_t seq, const uint8_t *payload, uint16_t payload_len);

/**
 * @brief 从字节流中解析协议帧
 * @param raw 原始字节流
 * @param raw_len 字节流长度
 * @param msg 输出解析结果
 * @return 解析到的帧长度, 0 数据不足, -1 校验失败/格式错误
 */
int proto_parse_frame(const uint8_t *raw, int raw_len, ProtoMessage *msg);

/**
 * @brief 计算校验和 (异或和)
 * @param data 数据
 * @param len 长度
 * @return 校验和
 */
uint8_t proto_checksum(const uint8_t *data, int len);

/**
 * @brief 获取协议帧长度
 * @param frame 已构建的帧
 * @return 帧总长度
 */
static inline int proto_frame_len(const ProtoFrame *frame) {
    return PROTO_HEADER_SIZE + frame->header.payload_len + PROTO_CHECKSUM_SIZE;
}

/**
 * @brief 验证魔数
 */
static inline int proto_verify_magic(const uint8_t *data) {
    return (data[0] == PROTO_MAGIC_1 && data[1] == PROTO_MAGIC_2);
}

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
