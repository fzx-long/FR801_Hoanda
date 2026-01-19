/*********************************************************************
 * @file tpms_logic.h
 * @author Fanzx (1456925916@qq.com)
 * @brief 用于处理 TPMS 业务逻辑的头文件
 * @version 0.1
 * @date 2026-01-06
 * 
 * @copyright Copyright (c) 2026
 * 
 */


#ifndef TPMS_LOGIC_H
#define TPMS_LOGIC_H
/****************************Include***********************************/
#include <stdint.h>
#include <stdbool.h>
#include "TPMS.h"
/*****************************Type Define*******************************/
/* 业务阈值可按车型调整 */
#define TPMS_PRESSURE_MIN_KPA_X100 15000  /* 150 kPa */
#define TPMS_PRESSURE_MAX_KPA_X100 35000  /* 350 kPa */
#define TPMS_TEMP_MAX_C            85     /* 过温门限 */
#define TPMS_BATT_MIN_V_X100       250    /* 2.50 V 低电门限 */
#define TPMS_OFFLINE_CHECK_MS      10000  /* 轮询周期 */
#define TPMS_OFFLINE_ROUNDS        3      /* 超过 N 轮未收到判离线 */

/*********************************************************************
 * @brief TPMS 报警类型枚举
 */
typedef enum {
    TPMS_ALARM_NONE = 0,
    TPMS_ALARM_PRESSURE_LOW,
    TPMS_ALARM_PRESSURE_HIGH,
    TPMS_ALARM_TEMP_HIGH,
    TPMS_ALARM_BATT_LOW,
    TPMS_ALARM_DATA_INVALID,
    TPMS_ALARM_OFFLINE,
} tpms_alarm_t;

/*********************************************************************
 * @brief TPMS 传感器槽位状态结构体
 */
typedef struct {
    bool has_data;
    tpms_alarm_t alarm;
    TPMS_Frame last_frame;
} tpms_logic_slot_t;

/*****************************Function Prototypes*******************************/
/*********************************************************************
 * @brief TPMS 业务逻辑初始化
 */
void TPMS_Biz_Init(void);

/*********************************************************************
 * @brief 处理接收到的 TPMS 数据帧
 * @param slot 传感器槽位索引
 * @param frame 指向接收到的 TPMS 数据帧的指针
 */
void TPMS_Biz_OnFrame(uint8_t slot, const TPMS_Frame *frame);

/*********************************************************************
 * @brief 获取指定槽位的 TPMS 状态
 * @param slot 传感器槽位索引
 * @param out 指向用于存储槽位状态的结构体指针
 * @retval true 成功获取状态
 * @retval false 参数无效或获取失败
 */
bool TPMS_Biz_Get_Status(uint8_t slot, tpms_logic_slot_t *out);

#endif
