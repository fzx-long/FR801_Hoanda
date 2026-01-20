/**
 * @file ble_function.h
 * @author your name (you@domain.com)
 * @brief    用于处理BLE_SOC 相关功能的头文件
 * @version 0.1
 * @date 2025-12-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BLE_FUNCTION_H__
#define __BLE_FUNCTION_H__

#include <stdint.h>

/* CMD 分流处理 */
/* FE: APP 标准下行；FD: 智能协议 */
void BleFunc_FE_Connect(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_Defences(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_AntiTheft(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_PhoneMessage(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_RssiStrength(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_CarSearch(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_FactorySettings(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_BleUnpair(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_AddNfc(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_DeleteNfc(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_SearchNfc(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_OilDefence(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_OilCarSearch(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_SetBootLock(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_SetNfc(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_SetSeatLock(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_SetCarMute(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_SetMidBoxLock(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_SetEmergencyMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_GetPhoneMac(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_SetUnlockMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FE_ChargeDisplay(uint16_t cmd, const uint8_t* payload, uint8_t len);

void BleFunc_FD_AssistiveTrolley(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_DelayedHeadlight(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetChargingPower(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetPGearMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetChordHornMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetRgbLightMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetAuxiliaryParking(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetIntelligentSwitch(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_ParamSynchronize(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_ParamSynchronizeChange(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetDefaultMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetVichleGurdMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetAutoReturnMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetEbsSwitch(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetESaveMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetDynMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetSportMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetLostMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetTcsSwitch(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetSideStand(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetBatteryParameter(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetUpdataApp(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetHdcMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetHhcMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetStartAbility(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetSportPowerSpeed(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetEcoMode(uint16_t cmd, const uint8_t* payload, uint8_t len);
void BleFunc_FD_SetRadarSwitch(uint16_t cmd, const uint8_t* payload, uint8_t len);

/**
 * @brief UART 收到 MCU->SOC 回包后上报（用于 BLE 侧判断 MCU 是否回包）
 * @note 由 user_task.c 在完成帧校验/解析后调用
 */
void BleFunc_OnMcuUartFrame(uint16_t       sync,
                            uint16_t       feature,
                            uint16_t       id,
                            const uint8_t* data,
                            uint16_t       data_len,
                            uint8_t        crc_ok);

/**
 * @brief RSSI 距离状态变化回调（由 rssi_check 模块触发）
 *
 * @why
 * - 用于实现“RSSI 确认近距离后，向 MCU 下发触发命令”的业务链路。
 * - 仅在距离状态发生变化时触发，避免在 RSSI 高频回调里反复下发。
 */
void BleFunc_RSSI_DistanceChangeCb(uint8_t conidx,
                                   uint8_t new_distance,
                                   int16_t filtered_rssi,
                                   int8_t  raw_rssi);

/**
 * @brief RSSI 采集到就下发 MCU（发送滤波值，不做阈值/距离判定）
 *
 * @why
 * - 你要求“不做阈值，但要滤波后直接发”。
 * - 该接口在 GAP_EVT_LINK_RSSI 收到 raw RSSI 后，读取 EMA 滤波结果并下发：FF01/0x0118，payload=1B flt_rssi + 6B MAC。
 */
void BleFunc_RSSI_RawInd(uint8_t conidx, int8_t raw_rssi);

#endif // __BLE_FUNCTION_H__
