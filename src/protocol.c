#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "protocol.h"

/*
 * 协议模块实现 - 简单二进制帧协议
 * 
 * 设计要点:
 * 1. 头部固定 8 字节，便于流式解析
 * 2. 小端序传输 (STM32 端需对应处理)
 * 3. 异或校验简单高效
 * 4. 支持粘包/半包解析
 */

uint8_t proto_checksum(const uint8_t *data, int len) {
    uint8_t cs = 0;
    for (int i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

int proto_build_frame(ProtoFrame *frame, uint8_t cmd, uint8_t flags,
                      uint16_t seq, const uint8_t *payload, uint16_t payload_len) {
    if (!frame) {
        return -1;
    }
    if (payload_len > PROTO_MAX_PAYLOAD) {
        fprintf(stderr, "[PROTO] Payload too large: %d > %d\n", payload_len, PROTO_MAX_PAYLOAD);
        return -1;
    }

    /* 填充头部 */
    frame->header.magic1 = PROTO_MAGIC_1;
    frame->header.magic2 = PROTO_MAGIC_2;
    frame->header.cmd = cmd;
    frame->header.flags = flags;
    frame->header.seq = seq;             /* 主机字节序 (小端) */
    frame->header.payload_len = payload_len;

    /* 填充载荷 */
    if (payload && payload_len > 0) {
        memcpy(frame->payload, payload, payload_len);
    }

    /* 计算校验和: 头部 + 载荷 */
    int header_cs_len = PROTO_HEADER_SIZE + payload_len;
    uint8_t temp_buf[PROTO_HEADER_SIZE + PROTO_MAX_PAYLOAD];
    memcpy(temp_buf, &frame->header, PROTO_HEADER_SIZE);
    if (payload && payload_len > 0) {
        memcpy(temp_buf + PROTO_HEADER_SIZE, payload, payload_len);
    }
    frame->checksum = proto_checksum(temp_buf, header_cs_len);

    frame->total_len = PROTO_HEADER_SIZE + payload_len + PROTO_CHECKSUM_SIZE;
    return frame->total_len;
}

int proto_parse_frame(const uint8_t *raw, int raw_len, ProtoMessage *msg) {
    if (!raw || raw_len < PROTO_HEADER_SIZE) {
        return 0;  /* 数据不足 */
    }

    /* 验证魔数 */
    if (!proto_verify_magic(raw)) {
        /* 跳过第一个字节，继续搜索 */
        return -1;
    }

    /* 解析头部 */
    ProtoHeader hdr;
    memcpy(&hdr, raw, PROTO_HEADER_SIZE);

    uint16_t payload_len = hdr.payload_len;  /* 小端序 */

    /* 检查完整帧长度 */
    int frame_len = PROTO_HEADER_SIZE + payload_len + PROTO_CHECKSUM_SIZE;
    if (raw_len < frame_len) {
        return 0;  /* 半包，数据不完整 */
    }

    /* 验证校验和 */
    uint8_t expected_cs = proto_checksum(raw, frame_len - PROTO_CHECKSUM_SIZE);
    uint8_t actual_cs = raw[frame_len - PROTO_CHECKSUM_SIZE];
    if (expected_cs != actual_cs) {
        fprintf(stderr, "[PROTO] Checksum mismatch: expected 0x%02X, got 0x%02X\n",
                expected_cs, actual_cs);
        return -1;
    }

    /* 填充输出消息 */
    if (msg) {
        msg->cmd = hdr.cmd;
        msg->flags = hdr.flags;
        msg->seq = hdr.seq;
        msg->payload = raw + PROTO_HEADER_SIZE;
        msg->payload_len = payload_len;
    }

    return frame_len;
}
