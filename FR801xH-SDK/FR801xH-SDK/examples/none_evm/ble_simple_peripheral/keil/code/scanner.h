/**
 * @file scanner.h
 * @brief 用于生成扫描功能的头文件（OOP 风格）
 * @version 0.1
 * @date 2025-12-31
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __SCANNER_H__
#define __SCANNER_H__
#include <stdint.h>
#include "gap_api.h"  // 引入 GAP API

/* ============ 操作函数指针结构体 ============ */
typedef struct ble_central_ops_v2_t{
        void (*scan_start) (void);                      //开始扫描函数指针
        void (*scan_stop) (void);                       //停止扫描函数指针
        void (*get_rssi) (void);                        //获取RSSI函数指针（打印到串口）
        void (*get_mac) (void);                         //获取MAC地址函数指针（打印到串口）
        uint8_t * (*get_adv_data) (uint8_t * data);    //获取广播数据函数指针（复制到 data）
}BLE_Central_Ops_V2_t;

/* ============ 设备基础数据结构 ============ */
typedef struct{
        uint8_t  scaned;                        //扫描状态（0=未扫描，1=已扫描到设备）
        uint8_t  scan_mac[6];                    //最后扫描到的设备地址（小端序）
        uint8_t  rssi;                          //最后扫描到的信号强度（dBm）
        struct ble_central_ops_v2_t  ops;      //操作函数指针结构体（OOP 封装）
}BLE_Central_Base_t;

/* ============ 公共接口函数 ============ */

/**
 * @brief 创建并初始化 BLE Central 扫描器实例
 * @param device 设备结构体指针（由调用者分配内存）
 */
void BLE_Scanner_Create(BLE_Central_Base_t *device);

/**
 * @brief GAP 事件处理函数（需要在 app_gap_evt_cb 中调用）
 * @param p_event GAP 事件指针
 * 
 * @note 在 proj_main.c 的 app_gap_evt_cb() 函数开头添加：
 *       BLE_Scanner_Event_Handler(p_event);
 */
void BLE_Scanner_Event_Handler(gap_event_t *p_event);

#endif // __SCANNER_H__
