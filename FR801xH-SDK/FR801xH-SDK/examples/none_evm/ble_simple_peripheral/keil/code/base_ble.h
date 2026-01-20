/*********************************************************************
 * @file base_ble.h
 * @author Fanzx (1456925916@qq.com)
 * @brief  基础 BLE 设备类型定义与接口声明
 * @version 0.1
 * @date 2026-01-06
 *
 * @copyright Copyright (c) 2026
 *
 */
/**********************************Include**************************************
 */
#ifndef __BASE_BLE__
#define __BASE_BLE__
#include <stdint.h>
/*****************************Type Define**************************************/
#define CONNCT_DEIVCE_MAX_NUM 3   // 最大连接设备数3 \
                                  // 用于判断是否有空闲连接设备位置
extern uint8_t connct_device_num; // 当前已连接设备数

/*********************************************************************
 * @brief BLE 外设操作函数指针结构体
 */
typedef struct {
    void (*pairing)(void);                    // 配对函数指针
    void (*adv_start)(void);                  // 开始广播函数指针
    void (*adv_stop)(void);                   // 停止广播函数指针
    void (*updata_AES_key)(uint8_t* key);     // 更新AES密钥函数指针
    void (*updata_SN_DATA)(uint8_t* sn_data); // 更新SN数据函数指针
} BLE_Peripheral_Ops_V2_t;

/*********************************************************************
 * @brief BLE 外设基础结构体
 */
typedef struct {
    char    name[20];                                         // 设备名称
    uint8_t connected;                                        // 连接状态
    uint8_t (*protocol)(uint8_t* data, uint16_t len);         // 协议处理函数指针
    uint8_t (*send_data_app)(uint8_t* data, uint16_t len);    // 数据发送函数指针
    uint8_t (*receive_data_app)(uint8_t* data, uint16_t len); // 数据接收函数指针
    BLE_Peripheral_Ops_V2_t ops;                              // 操作函数指针结构体
                                                              // 主要是对蓝牙底层驱动的操作
} BLE_Peripheral_Base_t;
/*****************************Function
 * Declare**********************************/
/*********************************************************************
 * @brief  初始化连接设备
 * @param  device: 指向要初始化的设备结构体指针
 * 也就是上面的connected_devices数组
 */
void ble_Device_init(BLE_Peripheral_Base_t* device);
#endif // !__BASE_BLE__
