/*********************************************************************
 * @file tpms_logic.c
 * @author Fanzx (1456925916@qq.com)
 * @brief 用于处理 TPMS 业务逻辑的实现文件
 * @version 0.1
 * @date 2026-01-06
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "tpms_logic.h"
#include "co_printf.h"
#include "os_timer.h"
#include <string.h>

static tpms_logic_slot_t s_slots[TPMS_SENSOR_MAX];
static uint32_t s_round = 0;
static uint32_t s_last_round[TPMS_SENSOR_MAX];
static os_timer_t s_timer;

/*********************************************************************
 * @brief 评估 TPMS 数据帧的报警状态
 * @param f 指向 TPMS 数据帧的指针
 * @retval 返回对应的报警类型
 */
static tpms_alarm_t eval_frame(const TPMS_Frame *f)
{
    if (f == NULL || !f->valid) {
        return TPMS_ALARM_DATA_INVALID;
    }

    if (f->pressure_kpa_x100 < TPMS_PRESSURE_MIN_KPA_X100) {
        return TPMS_ALARM_PRESSURE_LOW;
    }
    if (f->pressure_kpa_x100 > TPMS_PRESSURE_MAX_KPA_X100) {
        return TPMS_ALARM_PRESSURE_HIGH;
    }
    if (f->temperature_c > TPMS_TEMP_MAX_C) {
        return TPMS_ALARM_TEMP_HIGH;
    }
    if (f->battery_v_x100 < TPMS_BATT_MIN_V_X100) {
        return TPMS_ALARM_BATT_LOW;
    }
    return TPMS_ALARM_NONE;
}

/*********************************************************************
 * @brief 轮询定时器回调，检查各槽位离线状态
 * @param arg 定时器参数（未使用）
 */
static void timer_cb(void *arg)
{
    (void)arg;
    s_round++;
    for (uint8_t i = 0; i < TPMS_SENSOR_MAX; i++) {
        if (!s_slots[i].has_data) {
            continue;
        }
        if ((s_round - s_last_round[i]) >= TPMS_OFFLINE_ROUNDS) {
            s_slots[i].alarm = TPMS_ALARM_OFFLINE;
        }
    }
    os_timer_start(&s_timer, TPMS_OFFLINE_CHECK_MS, 0);
}

/*********************************************************************
 * @brief TPMS 业务逻辑初始化
 */
void TPMS_Biz_Init(void)
{
    memset(s_slots, 0, sizeof(s_slots));
    for (uint8_t i = 0; i < TPMS_SENSOR_MAX; i++) {
        s_last_round[i] = 0;
    }
    TPMS_Init();
    os_timer_init(&s_timer, timer_cb, NULL);
    os_timer_start(&s_timer, TPMS_OFFLINE_CHECK_MS, 0);
}
/*********************************************************************
 * @brief 处理接收到的 TPMS 数据帧
 * @param slot 传感器槽位索引
 * @param frame 指向接收到的 TPMS 数据帧的指针
 */
void TPMS_Biz_OnFrame(uint8_t slot, const TPMS_Frame *frame)
{
    if (slot >= TPMS_SENSOR_MAX) return;
    s_slots[slot].has_data = true;
    s_slots[slot].last_frame = *frame;
    s_slots[slot].alarm = eval_frame(frame);
    s_last_round[slot] = s_round;

    if (s_slots[slot].alarm != TPMS_ALARM_NONE) {
        co_printf("[TPMS] alarm slot=%u code=%u\r\n", slot, (unsigned)s_slots[slot].alarm);
    }
}

/*********************************************************************
 * @brief 获取指定槽位的 TPMS 状态
 * @param slot 传感器槽位索引
 * @param out 指向用于存储槽位状态的结构体指针
 * @retval true 成功获取状态
 * @retval false 参数无效或获取失败
 */
bool TPMS_Biz_Get_Status(uint8_t slot, tpms_logic_slot_t *out)
{
    if (slot >= TPMS_SENSOR_MAX || out == NULL) return false;
    *out = s_slots[slot];
    return true;
}
