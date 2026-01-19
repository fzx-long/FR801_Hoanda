/*********************************************************************
 * @file base_ble.c
 * @author Fanzx (1456925916@qq.com)
 * @brief   基础 BLE 设备类型实现文件
 * @version 0.1
 * @date 2026-01-06
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "base_ble.h"
#include <string.h>
#include "protocol.h"
#include "co_printf.h" // 假设有打印函数

uint8_t connct_device_num = 0; //当前已连接设备数
/*********************************************************************
 * @brief 连接设备数组
 */
static BLE_Peripheral_Base_t connected_devices[CONNCT_DEIVCE_MAX_NUM] =
{
    [0]= {0},
    [1]= {0},
    [2]= {0}
};

/*********************************************************************
 * @brief 初始化连接设备
 * @param device 指向要初始化的设备结构体指针 
 *               也就是上面的connected_devices数组
 */
void ble_Device_init(BLE_Peripheral_Base_t *device)
{   
    for(int i=0; i<CONNCT_DEIVCE_MAX_NUM; i++)
    {
        if(connected_devices[i].connected == 0)
        {
            memcpy(&connected_devices[i], device, sizeof(BLE_Peripheral_Base_t));
            connected_devices[i].connected = 1;
            connct_device_num++;
            break;
        }
    }
}
