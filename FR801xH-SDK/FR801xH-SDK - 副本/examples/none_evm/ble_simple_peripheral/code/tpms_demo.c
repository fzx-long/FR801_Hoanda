/**
 * @file tpms_demo.c
 * @brief TPMS 广播解析示例（占位，等待正式协议文档）
 */
#include "tpms_demo.h"
#include "co_printf.h"
#include <string.h>

/* 简单 HEX dump，便于拿到协议后对比 */
static void tpms_dump_hex(const uint8_t *p, uint8_t len)
{
    co_printf("[TPMS] RAW len=%d: ", len);
    for (uint8_t i = 0; i < len; i++) {
        co_printf("%02X", p[i]);
    }
    co_printf("\r\n");
}

/* TLV 解析基础：遍历 AD 结构，找到厂商数据或 Service Data */
static void tpms_parse_adv(const uint8_t *data, uint8_t len)
{
    uint8_t idx = 0;
    while (idx + 1 < len) {
        uint8_t field_len  = data[idx];
        if (field_len == 0 || idx + 1 + field_len > len) {
            break;
        }
        uint8_t ad_type    = data[idx + 1];
        const uint8_t *val = &data[idx + 2];
        uint8_t val_len    = field_len - 1;

        // 示例：抓厂商数据 (0xFF) 或 Service Data (0x16 / 0x21)
        if (ad_type == 0xFF) {
            co_printf("[TPMS] Manufacturer Data len=%d\r\n", val_len);
            tpms_dump_hex(val, val_len);
        } else if (ad_type == 0x16 || ad_type == 0x21) {
            co_printf("[TPMS] Service Data (type=0x%02X) len=%d\r\n", ad_type, val_len);
            tpms_dump_hex(val, val_len);
        }

        // TODO: 拿到正式协议后，在此处解析压力/温度/电量等字段

        idx += (field_len + 1);
    }
}

void tpms_handle_adv(const uint8_t *data, uint8_t len)
{
    if (data == NULL || len == 0) {
        return;
    }
    // 仅打印/解析，不做 MAC/RSSI 输出
    tpms_parse_adv(data, len);
}
