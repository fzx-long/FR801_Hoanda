/**
 * @file tpms_demo.h
 * @brief TPMS 广播解析示例（占位，等待正式协议文档）
 */
#ifndef TPMS_DEMO_H
#define TPMS_DEMO_H

#include <stdint.h>

/**
 * @brief 处理一条广播数据（从 scanner 传入，长度<=31）
 * @param data  广播数据指针
 * @param len   数据长度
 */
void tpms_handle_adv(const uint8_t *data, uint8_t len);

#endif // TPMS_DEMO_H
