#ifndef __PARAM_SYNC_H__
#define __PARAM_SYNC_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 6.10 参数设置同步（设备 -> 手机 APP）
 * @details
 * - 设备每次连接并完成鉴权后，主动向 APP 同步一次（Cmd=0x64FD）。
 * - APP 会回一个确认（Cmd=0x6402，payload=ResultCode）。
 *
 * 为什么单独做成一个文件：
 * - 这属于“业务策略/同步策略”，和 ble_function.c 里大量的 FE/FD 指令处理解耦，后续扩字段更安全。
 */
void ParamSync_OnBleAuthed(uint8_t conidx);

/**
 * @brief 6.11 参数设置同步（有变化时，设备 -> 手机 APP）
 * @details
 * - 设备侧检测到状态变化时，主动推送（Cmd=0x66FD）。
 * - APP 回确认（Cmd=0x6602）。
 *
 * @note
 * - 当前工程里“变化检测”可能来自 MCU 上报或本地状态机；这里先提供 API，调用点由你后续接入。
 */
void ParamSync_NotifyChange(uint8_t intelligentSwitch1_vehicleStatus, uint16_t speed_01kmh);

/**
 * @brief 处理 APP 对 0x64FD/0x66FD 的回复帧（0x6402/0x6602）
 * @param conidx   连接索引
 * @param cmd      收到的命令（0x6402 或 0x6602）
 * @param payload  数据
 * @param len      数据长度
 */
void ParamSync_OnAppReply(uint8_t conidx, uint16_t cmd, const uint8_t *payload, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __PARAM_SYNC_H__ */
