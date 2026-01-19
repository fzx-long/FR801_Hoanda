#ifndef PROTO_TIME6_BCD_H
#define PROTO_TIME6_BCD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Time6(6 bytes) helper.
 *
 * 原始设计：Time6 为 BCD（每个 nibble 都是 0~9）。
 * 但当前联调日志显示 App 端发送的 Time6 更像“纯二进制 BIN 的年月日时分秒”。
 * 例如：1A 01 06 11 0E 15 -> 26-01-06 17:14:21
 *
 * 为了兼容：
 * - 优先按 BCD 解析（完全符合 BCD 约束时）
 * - 若不是 BCD，则尝试按 BIN 解析（字段范围合法时）
 *
 * 输出：12 位 ASCII 数字 + '\0'，格式为 "YYMMDDhhmmss"。
 */

static inline uint8_t proto_time6_bcd_to_str12(const uint8_t *time6, char *out13)
{
    if (time6 == 0 || out13 == 0) {
        return 0;
    }

    /* 1) 先判断是否为标准 BCD */
    uint8_t is_bcd = 1;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t b = time6[i];
        uint8_t hi = (uint8_t)(b >> 4);
        uint8_t lo = (uint8_t)(b & 0x0F);
        if (hi > 9 || lo > 9) {
            is_bcd = 0;
            break;
        }
    }

    if (is_bcd) {
        for (uint8_t i = 0; i < 6; i++) {
            uint8_t b = time6[i];
            uint8_t hi = (uint8_t)(b >> 4);
            uint8_t lo = (uint8_t)(b & 0x0F);
            out13[i * 2 + 0] = (char)('0' + hi);
            out13[i * 2 + 1] = (char)('0' + lo);
        }
        out13[12] = '\0';
        return 1;
    }

    /* 2) 非 BCD：尝试按 BIN(纯数值) 解析 YY MM DD hh mm ss */
    {
        uint8_t yy = time6[0];
        uint8_t mm = time6[1];
        uint8_t dd = time6[2];
        uint8_t hh = time6[3];
        uint8_t mi = time6[4];
        uint8_t ss = time6[5];

        /* 简单范围校验，避免把随机数据误判成时间 */
        if (yy > 99) {
            out13[0] = '\0';
            return 0;
        }
        if (mm < 1 || mm > 12 || dd < 1 || dd > 31 || hh > 23 || mi > 59 || ss > 59) {
            out13[0] = '\0';
            return 0;
        }

        /* 写两位十进制 */
        #define PROTO_PUT2(pos, v) do { \
                out13[(pos) + 0] = (char)('0' + ((uint8_t)(v) / 10u)); \
                out13[(pos) + 1] = (char)('0' + ((uint8_t)(v) % 10u)); \
            } while (0)

        PROTO_PUT2(0,  yy);
        PROTO_PUT2(2,  mm);
        PROTO_PUT2(4,  dd);
        PROTO_PUT2(6,  hh);
        PROTO_PUT2(8,  mi);
        PROTO_PUT2(10, ss);
        out13[12] = '\0';

        #undef PROTO_PUT2
        return 1;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* PROTO_TIME6_BCD_H */
