/**
 * @author fzx-long (1456925916@qq.com)
 * @brief 用于处理 BLE_SOC 相关功能的源文件
 * @version 0.1
 * @date 2025-12-31
 */
#include "ble_function.h"
#include "co_printf.h"

#include "proto_time6_bcd.h"
#include "protocol.h"
#include "gap_api.h"
#include "param_sync.h"

#include "rssi_check.h"

#include "usart_cmd.h"
#include "usart_device.h"
#include "os_timer.h"
#include "en_de_algo.h" // Add MD5 support

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* Define default behavior for unimplemented functions: 0=Error, 1=Success */
#define BLEFUNC_ALLOW_UNIMPLEMENTED_SUCCESS 0

// 预置 T-BOX KEY (24字节)
// 注意：实际生产中应从 Flash 配置区读取，此处为演�?key
static const uint8_t TBOX_KEY[24] = {'1', '2', '3', '4', '5', '6', '7', '8',
                                     '9', '0', '1', '2', '3', '4', '5', '6',
                                     '7', '8', '9', '0', '1', '2', '3', '4'};

/*
 * 0x01FE 鉴权 Token 说明�?
 * - 文档要求 Token = MD5(T-BOX_KEY) �?32 字节 HEX 字符串�?
 * - 量产建议：只保留 TBOX_KEY，并由设备侧计算 MD5�?
 * -
 * 联调阶段如果你“只�?token(32位hex)”但暂时拿不�?T-BOX_KEY，可临时打开下面的覆盖宏�?
 *   例如�?
 *     #define TBOX_TOKEN_MD5_ASCII "6F35E30C05DBE6D747EB938DF71863D1"
 *   为什么要这样做：否则设备算出来的 Local Calc Token 会和 App
 * 发来的不一致，必然鉴权失败并断连�?
 */
/*
 * [联调临时策略]
 * 你已提供 App 侧使用的 Token（MD5 后的 32 �?HEX 字符串）�?
 * 先把它固定下来用于鉴权通过，后续再替换为真�?TBOX_KEY 计算或从 Flash 读取�?
 */
#ifndef TBOX_TOKEN_MD5_ASCII
#define TBOX_TOKEN_MD5_ASCII "6F35E30C05DBE6D747EB938DF71863D1"
#endif

/*
 * 说明�?
 * - 这些 FE/FD 指令属于“APP -> 设备”的下行控制类命令�?
 * - 真实项目里通常会触�?IO/继电�?存储等敏感动作�?
 * - 为避免在未接入硬件逻辑前“回包成功但实际未执行”造成误判�?
 *   默认对涉及硬件动作的指令�?ResultCode=0x01（失败）�?
 * - 你联调阶段如果希望先�?APP 流程走通，可将
 * BLEFUNC_ALLOW_UNIMPLEMENTED_SUCCESS 设为 1�?
 */

/*
 * MCU 侧通道字段（feature）说明：
 * - 请求（SOC->MCU）通常使用 FF01
 * - 应答（MCU->SOC）在现有工程里通常使用 FF02（参�?user_task.c �?TPMS
 * 回包注释�?
 *
 * 之前事务匹配用的是“请�?feature”，会导�?MCU 回包�?FF02 时匹配失败，从而超时回
 * 0x01 �?APP�?
 */
#ifndef BLEFUNC_MCU_FEATURE_REQ
#if defined(BLEFUNC_MCU_FEATURE)
#define BLEFUNC_MCU_FEATURE_REQ BLEFUNC_MCU_FEATURE
#else
#define BLEFUNC_MCU_FEATURE_REQ SOC_MCU_FEATURE_FF01
#endif
#endif

#ifndef BLEFUNC_MCU_FEATURE_RESP
#define BLEFUNC_MCU_FEATURE_RESP SOC_MCU_FEATURE_FF02
#endif

/* 兼容历史代码：旧宏名仍可用（等价于请求通道�?*/
#ifndef BLEFUNC_MCU_FEATURE
#define BLEFUNC_MCU_FEATURE BLEFUNC_MCU_FEATURE_REQ
#endif

/* 等待 MCU 回包超时时间（ms�?*/
#ifndef BLEFUNC_MCU_TIMEOUT_MS
#define BLEFUNC_MCU_TIMEOUT_MS 500u
#endif

#ifndef ENABLE_NFC_ADD_SIMULATION
#define ENABLE_NFC_ADD_SIMULATION 1
#endif

/*
 * 开发联调：BLE FE 命令自动成功回包开�?
 *
 * @why
 * - 某些 FE 命令需要等�?MCU 回包后再通知 App（pending 模式）�?
 * - 联调/单板阶段可能还没�?MCU、或 MCU 逻辑未就绪，会导�?App 一直等不到回包�?
 * - 因此提供“自动成功回包”开关：SOC 侧直接回 ResultCode=0x00，让流程先跑通�?
 *
 * @note
 * - 默认值跟�?ENABLE_NFC_ADD_SIMULATION（保持历史行为不变）�?
 * - 量产阶段建议显式设置�?0，恢复严格等�?MCU 回包�?
 */
#ifndef BLEFUNC_FE_AUTO_REPLY_ENABLE
#define BLEFUNC_FE_AUTO_REPLY_ENABLE (ENABLE_NFC_ADD_SIMULATION)
#endif

/* 仅对 RSSI Strength(0x07FE) 自动回包开关（需
 * BLEFUNC_FE_AUTO_REPLY_ENABLE=1�?*/
#ifndef BLEFUNC_FE_AUTO_REPLY_RSSI_STRENGTH
#define BLEFUNC_FE_AUTO_REPLY_RSSI_STRENGTH (1)
#endif

/*
 * 开发阶段临时策略：Connect(0x01FE) 接受任意 Token 直接鉴权通过�?
 *
 * @why
 * - 你当前处于开发联调阶段，还不需要把 App/后端 �?Token 算法完全对齐�?
 * - 但仍需要保留“加密收�?加密回包”链路，确保协议栈与加密模块联调正常�?
 * - 因此这里仅跳�?Token 比对（鉴权判定），不改变解密/加密流程�?
 *
 * @note
 * - 量产/集成阶段务必将该宏置 0，恢复严格鉴权；否则任何 Token 都会被放行�?
 */
#ifndef BLEFUNC_DEV_ACCEPT_ANY_TOKEN
#define BLEFUNC_DEV_ACCEPT_ANY_TOKEN 1
#endif

/* ====== MCU Transaction: 链式指令支持（用�?0x06FD 这类“一�?BLE 命令拆成多条
 * MCU 命令”的场景�?====== */
#define BLEFUNC_MCU_CHAIN_NONE            (0u)
#define BLEFUNC_MCU_CHAIN_CHORD_HORN_06FD (1u)
#define BLEFUNC_MCU_CHAIN_GEAR_SET_FD     (2u)
#define BLEFUNC_MCU_CHAIN_RADAR_SET_FD    (3u)

typedef struct {
    bool active;
    bool timer_inited;
    uint8_t conidx;
    uint16_t reply_cmd;
    uint16_t req_feature;
    uint16_t resp_feature;
    uint16_t id;
    /*
     * 链式事务上下文：
     * - 某些 BLE 指令（如 0x06FD）payload 是复合字段，MCU 协议要求拆成多条 1B
     * data �?CMD�?
     * - 这里�?chain_kind/chain_step 在收到第 1 �?MCU 回包后自动触发第 2 �?MCU
     * 下发�?
     */
    uint8_t chain_kind; /* BLEFUNC_MCU_CHAIN_* */
    uint8_t chain_step; /* 0=等待�?步回包，1=等待�?步回�?*/
    uint8_t
        chain_u8_0; /* 预留：当前用于保�?0x06FD �?volume�?B）或档位 speed(1B) */
    uint16_t chain_u16_0; /* 预留：当前用于保存“第 2 步”的 MCU 命令
                             ID（例�?speed_set�?*/
    os_timer_t timer;
} blefunc_mcu_pending_t;

static blefunc_mcu_pending_t s_mcu_pending;

/* RSSI->MCU 触发：每条链路进�?NEAR 只触发一次，离开 NEAR 后允许再次触�?*/
static uint8_t s_rssi_near_latched[RSSI_MAX_CONN] = {0};

/* 业务约定：RSSI 确认近距离后，下发给 MCU 的命令号（走 FF01 通道�?*/
#ifndef BLEFUNC_CMD_RSSI_NEAR_CONFIRM
#define BLEFUNC_CMD_RSSI_NEAR_CONFIRM (0x0118u)
#endif

void BleFunc_RSSI_RawInd(uint8_t conidx, int8_t raw_rssi)
{
    if (conidx >= RSSI_MAX_CONN) { return; }

    /*
     * @why
     * - 你选择“滤波后再发，不要阈值”�?
     * - RSSI_Check_Update() 已在 GAP_EVT_LINK_RSSI 分支里先执行�?
     *   这里直接读取 EMA 滤波后的值作为下发值�?
     */
    int16_t flt16 = RSSI_Check_Get_Filtered(conidx);
    if (flt16 > 127) flt16 = 127;
    if (flt16 < -127) flt16 = -127;
    int8_t flt_rssi = (int8_t)flt16;

    /*
     * payload 约定（你指定）：
     * - 1B: RSSI 滤波值（int8，EMA 输出；二进制补码形式，直接透传�?
     * - 6B: 对端 MAC
     */
    uint8_t mcu_payload[7];
    uint8_t mac6[6] = {0};
    (void)RSSI_Check_Get_Peer_Addr(conidx, mac6);

    mcu_payload[0] = (uint8_t)flt_rssi;
    memcpy(&mcu_payload[1], mac6, 6);

    uint8_t ok =
        SocMcu_Frame_Send(SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_FF01,
                          (uint16_t)BLEFUNC_CMD_RSSI_NEAR_CONFIRM, mcu_payload,
                          (uint16_t)sizeof(mcu_payload));
    (void)ok;
}

void BleFunc_RSSI_DistanceChangeCb(uint8_t conidx, uint8_t new_distance,
                                   int16_t filtered_rssi, int8_t raw_rssi)
{
    if (conidx >= RSSI_MAX_CONN) { return; }
    (void)new_distance;
    (void)filtered_rssi;
    (void)raw_rssi;

    /*
     * @why
     * - `gap_rssi_ind()` 属于实时回调，RSSI 会高频波动�?
     * - 这里仅在“距离状态变化”时触发；并且进�?NEAR 只下发一次，避免重复刷串口�?
     */
    if (new_distance == (uint8_t)RSSI_DIST_NEAR) {
        if (s_rssi_near_latched[conidx]) { return; }
        s_rssi_near_latched[conidx] = 1;

        /*
         * payload 约定（你指定）：
         * - 1B: RSSI 原始值（int8，二进制补码形式，直接透传�?
         * - 6B: 对端 MAC
         */
        uint8_t mcu_payload[7];
        uint8_t mac6[6] = {0};
        (void)RSSI_Check_Get_Peer_Addr(conidx, mac6);
        mcu_payload[0] = (uint8_t)raw_rssi;
        memcpy(&mcu_payload[1], mac6, 6);

        uint8_t ok =
            SocMcu_Frame_Send(SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_FF01,
                              (uint16_t)BLEFUNC_CMD_RSSI_NEAR_CONFIRM,
                              mcu_payload, (uint16_t)sizeof(mcu_payload));
        (void)ok;
        return;
    }

    /* 离开 NEAR：清门禁，下一次进�?NEAR 允许再次触发 */
    s_rssi_near_latched[conidx] = 0;
}

/* 补充缺失的辅助函数定�?*/
/*********************************************************************
 * @brief 构�?FE 类指令的回复命令�?
 * @param cmd {placeholder}
 * @retval {placeholder}
 */
static uint16_t BleFunc_MakeReplyCmd_FE(uint16_t cmd)
{
    /* FE 类指�?APP->Dev)通常回复 0xXX01 */
    return (cmd & 0xFF00) | 0x01;
}

static uint16_t BleFunc_MakeReplyCmd_FD(uint16_t cmd)
{
    /* FD 类指�?Dev->APP)通常回复 0xXX02 */
    return (cmd & 0xFF00) | 0x02;
}

/* MCU 主动上报 -> APP 推送使用的 CMD（FD 类，设备->APP 数据同步/上报�?*/
#ifndef BLEFUNC_MCU_PUSH_CMD
#define BLEFUNC_MCU_PUSH_CMD 0x13FDu
#endif

/* 最大连接数：与工程里的 SP_MAX_CONN_NUM/Protocol.c 保持一�?*/
#ifndef BLEFUNC_MAX_CONN
#define BLEFUNC_MAX_CONN 3u
#endif

/* --------------------------------------------------------------------------
 * TPMS(胎压) 上报联调�?x0117 payload �?0 时使用模拟数�?
 *
 * @why
 * - 你当前抓�?MCU->SOC �?`feature=0xFF02, id=0x0117, data_len=17` �?payload
 * �?0�? App 侧无法展示有效胎压�?
 * - 为了先跑�?App 联调链路（Notify 上报 + App 解析），这里做一个“全 0 ->
 * 模拟数据”的兜底�?
 * - 使用宏定义，方便后续你直接改参数或一键关闭�?
 *
 * @note
 * - 后续 MCU 有真实数据时，应关闭 BLEFUNC_TPMS_SIM_ENABLE 或删除该兜底�?
 * - 17 字节格式：Wheel(1)+ID(4)+Press(2)+Temp(1)+Volt(2)+Status(1)+MAC(6)
 *   多字节字段按大端（BE）�?
 */
#ifndef BLEFUNC_TPMS_SIM_ENABLE
#define BLEFUNC_TPMS_SIM_ENABLE 1
#endif

#ifndef BLEFUNC_TPMS_SIM_WHEEL
#define BLEFUNC_TPMS_SIM_WHEEL 0x00u
#endif

#ifndef BLEFUNC_TPMS_SIM_SENSOR_ID
#define BLEFUNC_TPMS_SIM_SENSOR_ID 0x11223344u
#endif

/* 压力/电压数值单位由 MCU/协议定义；这里给一个“非 0 的合理值”用于联�?*/
#ifndef BLEFUNC_TPMS_SIM_PRESSURE
#define BLEFUNC_TPMS_SIM_PRESSURE 250u
#endif

#ifndef BLEFUNC_TPMS_SIM_TEMP
#define BLEFUNC_TPMS_SIM_TEMP 25u
#endif

#ifndef BLEFUNC_TPMS_SIM_VOLT
#define BLEFUNC_TPMS_SIM_VOLT 3000u
#endif

#ifndef BLEFUNC_TPMS_SIM_STATUS
#define BLEFUNC_TPMS_SIM_STATUS 0x00u
#endif

#ifndef BLEFUNC_TPMS_SIM_MAC0
#define BLEFUNC_TPMS_SIM_MAC0 0xA1u
#define BLEFUNC_TPMS_SIM_MAC1 0xB2u
#define BLEFUNC_TPMS_SIM_MAC2 0xC3u
#define BLEFUNC_TPMS_SIM_MAC3 0xD4u
#define BLEFUNC_TPMS_SIM_MAC4 0xE5u
#define BLEFUNC_TPMS_SIM_MAC5 0xF6u
#endif

#define BLEFUNC_U16_BE_HI(v) ((uint8_t)(((uint16_t)(v) >> 8) & 0xFFu))
#define BLEFUNC_U16_BE_LO(v) ((uint8_t)((uint16_t)(v) & 0xFFu))
#define BLEFUNC_U32_BE_B0(v) ((uint8_t)(((uint32_t)(v) >> 24) & 0xFFu))
#define BLEFUNC_U32_BE_B1(v) ((uint8_t)(((uint32_t)(v) >> 16) & 0xFFu))
#define BLEFUNC_U32_BE_B2(v) ((uint8_t)(((uint32_t)(v) >> 8) & 0xFFu))
#define BLEFUNC_U32_BE_B3(v) ((uint8_t)((uint32_t)(v) & 0xFFu))

static const uint8_t s_tpms_sim_payload_0117[17] = {
    (uint8_t)BLEFUNC_TPMS_SIM_WHEEL,
    BLEFUNC_U32_BE_B0(BLEFUNC_TPMS_SIM_SENSOR_ID),
    BLEFUNC_U32_BE_B1(BLEFUNC_TPMS_SIM_SENSOR_ID),
    BLEFUNC_U32_BE_B2(BLEFUNC_TPMS_SIM_SENSOR_ID),
    BLEFUNC_U32_BE_B3(BLEFUNC_TPMS_SIM_SENSOR_ID),
    BLEFUNC_U16_BE_HI(BLEFUNC_TPMS_SIM_PRESSURE),
    BLEFUNC_U16_BE_LO(BLEFUNC_TPMS_SIM_PRESSURE),
    (uint8_t)BLEFUNC_TPMS_SIM_TEMP,
    BLEFUNC_U16_BE_HI(BLEFUNC_TPMS_SIM_VOLT),
    BLEFUNC_U16_BE_LO(BLEFUNC_TPMS_SIM_VOLT),
    (uint8_t)BLEFUNC_TPMS_SIM_STATUS,
    (uint8_t)BLEFUNC_TPMS_SIM_MAC0,
    (uint8_t)BLEFUNC_TPMS_SIM_MAC1,
    (uint8_t)BLEFUNC_TPMS_SIM_MAC2,
    (uint8_t)BLEFUNC_TPMS_SIM_MAC3,
    (uint8_t)BLEFUNC_TPMS_SIM_MAC4,
    (uint8_t)BLEFUNC_TPMS_SIM_MAC5,
};

static void BleFunc_PushMcuFrameToAuthedApp(uint16_t feature, uint16_t id,
                                            const uint8_t *data,
                                            uint16_t data_len)
{
    /* payload = feature(2) + id(2) + data_len(2) + data(n) */
    /*
     * [稳定性] 该函数可能在 UART 任务/回调中被频繁调用，栈空间通常较小�?
     * 这里把大 buffer 从栈移到静态区，避免栈溢出导致 HardFault 重启�?
     * 注意：该缓冲非可重入；但当前转发路径为串行处理（单线程任务）�?
     */
    static uint8_t payload[240];
    const uint16_t hdr_len = 6u;
    uint16_t copy_len      = data_len;
    if (copy_len > (uint16_t)(sizeof(payload) - hdr_len)) {
        copy_len = (uint16_t)(sizeof(payload) - hdr_len);
    }

    payload[0] = (uint8_t)(feature & 0xFFu);
    payload[1] = (uint8_t)((feature >> 8) & 0xFFu);
    payload[2] = (uint8_t)(id & 0xFFu);
    payload[3] = (uint8_t)((id >> 8) & 0xFFu);
    payload[4] = (uint8_t)(copy_len & 0xFFu);
    payload[5] = (uint8_t)((copy_len >> 8) & 0xFFu);
    if (copy_len > 0u && data != NULL) {
        memcpy(&payload[hdr_len], data, copy_len);
    }

    for (uint8_t conidx = 0; conidx < (uint8_t)BLEFUNC_MAX_CONN; conidx++) {
        /* 只推送给已鉴权连接，避免未登录的连接收到业务上报 */
        if (!Protocol_Auth_IsOk(conidx)) { continue; }
        if (gap_get_connect_status(conidx) == 0) { continue; }
        (void)Protocol_Send_Unicast(conidx, (uint16_t)BLEFUNC_MCU_PUSH_CMD,
                                    payload, (uint16_t)(hdr_len + copy_len));
    }
}

static void BleFunc_SendResultToConidx(uint8_t conidx, uint16_t reply_cmd,
                                       uint8_t result_code)
{
    uint8_t resp_payload[1];
    resp_payload[0] = result_code;
    (void)Protocol_Send_Unicast(conidx, reply_cmd, resp_payload,
                                (uint16_t)sizeof(resp_payload));
}

/*
 * @brief 下发 MCU 前剥�?Time6
 *
 * @why
 * - 你要求“所有发�?MCU 的字段都不要时间”，MCU
 * 只关心业务字段（control/参数等）�?
 * - 工程�?BLE 下行大多�?payload �?Time(6) 开头，如果不统一剥离，会出现�?
 *   A) MCU �?1B/2B 参数解析时错�?
 *   B) MCU 不回�?-> BLE 事务超时
 */
static void BleFunc_StripTime6_ForMcu(const uint8_t **payload,
                                      uint16_t *payload_len)
{
    if (payload == NULL || payload_len == NULL) { return; }
    if (*payload == NULL || *payload_len < 6u) { return; }
    *payload     = &((*payload)[6]);
    *payload_len = (uint16_t)(*payload_len - 6u);
}

static uint16_t BleFunc_MapBleCmdToMcuId(uint16_t ble_cmd,
                                         const uint8_t *payload,
                                         uint16_t payload_len)
{
    /*
     * 关键点：BLE 下行�?cmd(�?0x0FFE/0x12FE) 不等�?MCU UART �?id�?
     * MCU 侧命令号�?usart_cmd.h 里的 CMD_* 为准�?
     */
    switch (ble_cmd) {
        case 0x63FDu:
            /*
             * 6.9 智能开关设置：BLE payload = Time6 + control(1) +
             * controlType(1) 优先识别 controlType，处理已知类型：
             * - controlType==0x01 -> EBS开�?动力回收)
             * - controlType==0x04 -> 充电显示器（CMD_CHARGE_DISPLAY_ON/OFF�?
             * - controlType==0x0A -> 三色氛围灯（CMD_Three_shooting_lamp_*�?
             * - controlType==0x02 ->
             * 鞍座感应开关（CMD_Saddle_sensing_on/off/default�?
             * - controlType==0x03 -> ABS（CMD_ABS_on/off/default�?
             * - controlType==0x05 ->
             * 位置灯（CMD_Position_light_on/off/default�?
             * 其它未识别的类型则按“动力回收（Power Recover）”的开/关映射�?
             */
            if (payload_len >= 8u && payload != NULL) {
                uint8_t control     = payload[6];
                uint8_t controlType = payload[7];
                if (controlType == 0x04u) {
                    return (control == 0x01u)
                               ? (uint16_t)CMD_CHARGE_DISPLAY_ON
                               : (uint16_t)CMD_CHARGE_DISPLAY_OFF;
                }
                if (controlType == 0x0Au) {
                    if (control == 0x01u)
                        return (uint16_t)CMD_Three_shooting_lamp_on;
                    if (control == 0x00u)
                        return (uint16_t)CMD_Three_shooting_lamp_off;
                    if (control == 0x02u)
                        return (uint16_t)CMD_Three_shooting_lamp_default;
                    return 0u;
                }
            }
            /* 兜底：按 control 映射为动力回收开�?*/
            if (payload_len >= 7u && payload != NULL) {
                uint8_t control = payload[6];
                if (control == 0x01u) return (uint16_t)CMD_POWER_RECOVER_ON;
                if (control == 0x00u) return (uint16_t)CMD_POWER_RECOVER_OFF;
            }
            return 0u;
        case 0x09FDu: /* 6.13 车辆防盗告警设置：MCU 侧复用油车设防命�?0x31 */
            (void)payload;
            (void)payload_len;
            return (uint16_t)CMD_OIL_VEHICLE_PREVENTION;
        case 0x02FDu: /* 助力推车模式 */
            if (payload_len >= 7u && payload != NULL) {
                uint8_t control = payload[6];
                if (control == 0x01u)
                    return (uint16_t)CMD_Assistive_trolley_mode_on;
                if (control == 0x00u)
                    return (uint16_t)CMD_Assistive_trolley_mode_off;
                if (control == 0x02u)
                    return (uint16_t)CMD_Assistive_trolley_mode_default;
            }
            return 0u;
        case 0x03FEu: /* 5.3 设防/撤防 (复用油车设防指令) */
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x00u)
                           ? (uint16_t)CMD_OIL_VEHICLE_UNPREVENTION
                           : (uint16_t)CMD_OIL_VEHICLE_PREVENTION;
            }
            return 0u;
        case 0x07FEu: /* 5.7 RSSI 范围设置 */
            return (uint16_t)CMD_BLE_RSSI_RANGE_SET;
        case 0x03FDu: /* 延时大灯 */
            if (payload_len >= 7u && payload != NULL) {
                uint8_t control = payload[6];
                if (control == 0x01u) return (uint16_t)CMD_Delayed_headlight_on;
                if (control == 0x00u)
                    return (uint16_t)CMD_Delayed_headlight_off;
                if (control == 0x02u)
                    return (uint16_t)CMD_Delayed_headlight_default;
                if (control == 0x03u)
                    return (uint16_t)CMD_Delayed_headlight_time_set;
            }
            return 0u;
        case 0x04FDu: /* 充电功率 */
            return (uint16_t)CMD_Charging_power_set;
        case 0x05FDu: /* 自动 P �?*/
            if (payload_len >= 7u && payload != NULL) {
                uint8_t control = payload[6];
                if (control == 0x01u) return (uint16_t)CMD_AUTO_P_GEAR_on;
                if (control == 0x00u) return (uint16_t)CMD_AUTO_P_GEAR_off;
                if (control == 0x02u) return (uint16_t)CMD_AUTO_P_GEAR_default;
                if (control == 0x03u) return (uint16_t)CMD_AUTO_P_GEAR_time_set;
            }
            return 0u;
        case 0x06FDu:
            /*
             * 6.6 和弦喇叭：BLE payload=Time6+soundSource+volume�?
             * MCU
             * 协议：CMD_Chord_horn_type_set(0x59)/CMD_Chord_horn_volume_set(0x60)
             * 均为 data 1B�? 因此�?BLE
             * 命令不能在这里“单条映射”，必须�?BleFunc_FD_SetChordHornMode()
             * 内拆分处理�?
             */
            (void)payload;
            (void)payload_len;
            return 0u;
        case 0x07FDu: /* 三色氛围灯：模式设置 */
            return (uint16_t)CMD_Three_shooting_lamp_mode_set;
        case 0x08FDu: /* 辅助倒车�?*/
            if (payload_len >= 7u && payload != NULL) {
                uint8_t control = payload[6];
                if (control == 0x01u)
                    return (uint16_t)CMD_Assistive_reversing_gear_on;
                if (control == 0x00u)
                    return (uint16_t)CMD_Assistive_reversing_gear_off;
                if (control == 0x02u)
                    return (uint16_t)CMD_Assistive_reversing_gear_default;
            }
            return 0u;
        case 0x0AFDu: /* 自动转向回位 */
            if (payload_len >= 7u && payload != NULL) {
                uint8_t control = payload[6];
                if (control == 0x01u)
                    return (uint16_t)CMD_Automatic_steering_reset_on;
                if (control == 0x00u)
                    return (uint16_t)CMD_Automatic_steering_reset_off;
                if (control == 0x02u)
                    return (uint16_t)CMD_Automatic_steering_reset_default;
            }
            return 0u;
        case 0x0BFEu: /* 添加 NFC 钥匙 */
            return (uint16_t)CMD_ADD_NFC_KEY;
        case 0x0CFEu: /* 删除 NFC 钥匙 */
            return (uint16_t)CMD_DELETE_NFC_KEY;
        case 0x0BFDu: /* EBS：开�?类型（data 段由 payload 透传�?*/
            return (uint16_t)CMD_EBS_switch_on;
        case 0x0CFDu: /* 低速档�?*/
            if (payload_len >= 1u && payload != NULL) {
                uint8_t control = payload[0];
                if (control == 0x01u) return (uint16_t)CMD_Low_speed_gear_on;
                if (control == 0x00u) return (uint16_t)CMD_Low_speed_gear_off;
                if (control == 0x02u)
                    return (uint16_t)CMD_Low_speed_gear_default;
            }
            return 0u;
        case 0x0DFDu: /* 中速档�?*/
            if (payload_len >= 1u && payload != NULL) {
                uint8_t control = payload[0];
                if (control == 0x01u) return (uint16_t)CMD_Medium_speed_gear_on;
                if (control == 0x00u)
                    return (uint16_t)CMD_Medium_speed_gear_off;
                if (control == 0x02u)
                    return (uint16_t)CMD_Medium_speed_gear_default;
            }
            return 0u;
        case 0x0EFDu: /* 高速档�?*/
            if (payload_len >= 1u && payload != NULL) {
                uint8_t control = payload[0];
                if (control == 0x01u) return (uint16_t)CMD_High_speed_gear_on;
                if (control == 0x00u) return (uint16_t)CMD_High_speed_gear_off;
                if (control == 0x02u)
                    return (uint16_t)CMD_High_speed_gear_default;
            }
            return 0u;
        case 0x0FFDu: /* 丢失模式 */
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x01u) ? (uint16_t)CMD_Lost_mode_on
                                             : (uint16_t)CMD_Lost_mode_off;
            }
            return 0u;
        case 0x10FDu: /* TCS */
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x01u) ? (uint16_t)CMD_TCS_switch_on
                                             : (uint16_t)CMD_TCS_switch_off;
            }
            return 0u;
        case 0x11FDu: /* 侧支�?*/
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x01u)
                           ? (uint16_t)CMD_Side_stand_switch_on
                           : (uint16_t)CMD_Side_stand_switch_off;
            }
            return 0u;
        case 0x12FDu: /* 电池类型与容量（先按 type_set 下发，data 段由 payload
                         透传�?*/
            return (uint16_t)CMD_Battery_type_set;
        case 0x14FDu: /* HDC 开�?*/
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x01u) ? (uint16_t)CMD_HDC_switch_on
                                             : (uint16_t)CMD_HDC_switch_off;
            }
            return 0u;
        case 0x15FDu: /* HHC 开�?*/
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x01u) ? (uint16_t)CMD_HHC_switch_on
                                             : (uint16_t)CMD_HHC_switch_off;
            }
            return 0u;
        case 0x0FFEu: /* 油车设防/解防 */
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x00u)
                           ? (uint16_t)CMD_OIL_VEHICLE_UNPREVENTION
                           : (uint16_t)CMD_OIL_VEHICLE_PREVENTION;
            }
            return 0u;
        case 0x08FEu: /* 5.8 手机蓝牙寻车 */
            /*
             * MCU 命令号未单独定义“电车寻车”，目前沿用油车寻车状态查询命令�?
             * payload(�?Time6) 原样透传�?MCU，由 MCU
             * 侧决定如何触发鸣�?双闪等动作�?
             */
            return (uint16_t)CMD_OIL_VEHICLE_FIND_STATUS;
        case 0x11FEu: /* 油车寻车声音/音量（沿�?MCU �?0x32，data 带音量） */
            return (uint16_t)CMD_OIL_VEHICLE_FIND_STATUS;
        case 0x12FEu: /* 尾箱�?*/
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x00u)
                           ? (uint16_t)CMD_OIL_VEHICLE_UNLOCK_TRUNK
                           : (uint16_t)CMD_OIL_VEHICLE_LOCK_TRUNK;
            }
            return 0u;
        case 0x13FEu: /* NFC 开�?*/
            return (uint16_t)CMD_NFC_SWITCH;
        case 0x14FEu: /* 座桶�?*/
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x00u) ? (uint16_t)CMD_VEHICLE_UNLOCK_SEAT
                                             : (uint16_t)CMD_VEHICLE_LOCK_SEAT;
            }
            return 0u;
        case 0x15FEu: /* 静音设置 */
            return (uint16_t)CMD_VEHICLE_MUTE_SETTING;
        case 0x16FEu: /* 中箱�?*/
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x00u)
                           ? (uint16_t)CMD_VEHICLE_UNLOCK_MIDDLE_BOX
                           : (uint16_t)CMD_VEHICLE_LOCK_MIDDLE_BOX;
            }
            return 0u;
        case 0x17FEu: /* 紧急模�?*/
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x01u)
                           ? (uint16_t)CMD_VEHICLE_EMERGENCY_MODE_LOCK
                           : (uint16_t)CMD_VEHICLE_EMERGENCY_MODE_UNLOCK;
            }
            return 0u;
        case 0x18FEu: /* len==6 获取 MAC；len==7 单控开�?*/
            if (payload_len == 6u) { return (uint16_t)CMD_BLE_MAC_READ; }
            if (payload_len == 7u) {
                return (uint16_t)CMD_SINGLE_CONTROL_UNLOCK;
            }
            return 0u;
        case 0x1AFEu: /* 充电显示器开�?*/
            if (payload_len >= 7u && payload != NULL) {
                return (payload[6] == 0x01u) ? (uint16_t)CMD_CHARGE_DISPLAY_ON
                                             : (uint16_t)CMD_CHARGE_DISPLAY_OFF;
            }
            return 0u;
        case 0x04FEu: { /* 蓝牙感应解锁开关 -> MCU BLE RSSI lock on/off */
            uint8_t status = 0u;
            if (payload != NULL) {
                if (payload_len >= 7u) {
                    status = payload[6]; /* Time6 + status */
                } else if (payload_len >= 1u) {
                    status = payload[payload_len - 1u]; /* fallback: last byte */
                }
            }
            return (status == 0x01u) ? (uint16_t)CMD_BLE_RSSI_LOCK_ON
                                     : (uint16_t)CMD_BLE_RSSI_LOCK_OFF;
        }
        default:
            return 0u;
    }
}

static void BleFunc_McuTxn_Timeout(void *parg)
{
    (void)parg;
    if (!s_mcu_pending.active) { return; }

    co_printf("[MCU_TXN] timeout: conidx=%u req_feature=0x%04X "
              "resp_feature=0x%04X id=0x%04X reply=0x%04X\\r\\n",
              (unsigned)s_mcu_pending.conidx,
              (unsigned)s_mcu_pending.req_feature,
              (unsigned)s_mcu_pending.resp_feature, (unsigned)s_mcu_pending.id,
              (unsigned)s_mcu_pending.reply_cmd);

    BleFunc_SendResultToConidx(s_mcu_pending.conidx, s_mcu_pending.reply_cmd,
                               0x01);
    s_mcu_pending.active      = false;
    s_mcu_pending.chain_kind  = BLEFUNC_MCU_CHAIN_NONE;
    s_mcu_pending.chain_step  = 0u;
    s_mcu_pending.chain_u16_0 = 0u;
}

/**
 * @brief 开启一个与 MCU 的异步交互事�?(Transaction)
 *
 * @details
 * 这是一个核心的"中间�?函数，用来处理【蓝牙请�?-> 转发 MCU -> 等待 MCU 回复 ->
 * 回复蓝牙】的全流程�? 它的主要逻辑如下�?
 * 1. 【防重入检查】：如果当前已经有一个指令在等待 MCU
 * 回复（s_mcu_pending.active 且未超时），则拒绝新指令，直接回失败�?
 *    这是为了防止串口阻塞或逻辑混乱，保证一问一答的顺序性�?
 * 2. 【指令映射】：蓝牙协议中的 CMD ID (�?0x0BFE) 可能�?MCU 串口协议定义�?ID
 * (�?0x20) 不同�? 这里会自动调�?BleFunc_MapBleCmdToMcuId 进行翻译�?
 * 3. 【发送请求】：调用底层驱动 SocMcu_Frame_Send 将数据打包通过 UART 发送给
 * MCU�?
 * 4. 【挂起等待】：
 *    - 将当前事务的状态（连接索引、期待的回复CMD、ID等）保存�?s_mcu_pending
 * 全局结构体中�?
 *    - 启动一个超时定时器 (BLEFUNC_MCU_TIMEOUT_MS)�?
 *    - 函数立即返回（异步）�?
 *
 * 后续流程�?
 * - 成功路径：MCU 在超时前回复串口数据 -> 触发 BleFunc_OnMcuUartFrame -> 匹配
 * ID -> 取消定时�?-> 回复 APP 成功数据�?
 * - 超时路径：定时器到期 -> 触发 BleFunc_McuTxn_Timeout -> 清除事务状�?-> 回复
 * APP 失败 (0x01)�?
 *
 * @param conidx      发起请求的蓝牙连接句柄（用于知道是谁发起的，将来回包给谁�?
 * @param reply_cmd   将来回复�?APP 时使用的 BLE CMD
 * ID（例如收�?0x0BFE，它�?0x0B01�?
 * @param feature     通道号（通常�?SOC_MCU_FEATURE_FF01�?
 * @param id          BLE 指令原始 ID (�?0x0BFE，将在此函数内尝试被翻译�?MCU ID)
 * @param payload     透传的数据内�?
 * @param payload_len 数据长度
 */
static void BleFunc_McuTxn_Start(uint8_t conidx, uint16_t reply_cmd,
                                 uint16_t feature, uint16_t id,
                                 const uint8_t *payload, uint16_t payload_len)
{
    /* 1. 忙检测：确保上一个指令已经结束，避免串口并发冲突 */
    if (s_mcu_pending.active) {
        co_printf("[MCU_TXN] busy, reject new cmd\\r\\n");
        BleFunc_SendResultToConidx(conidx, reply_cmd, 0x01);
        return;
    }

    if (!s_mcu_pending.timer_inited) {
        os_timer_init(&s_mcu_pending.timer, BleFunc_McuTxn_Timeout, NULL);
        s_mcu_pending.timer_inited = true;
    }

    /* 2. ID 映射：将 BLE 协议 ID (�?0x..FE) 转换�?MCU 串口协议 ID (usart_cmd.h
     * 中的 define) */
    uint16_t mcu_id = id;
    /* 通常 BLE 侧以 FE/FD 结尾的命令号不是 MCU 实际的指令号，需要翻�?*/
    if (((id & 0x00FFu) == 0x00FEu) || ((id & 0x00FFu) == 0x00FDu)) {
        mcu_id = BleFunc_MapBleCmdToMcuId(id, payload, payload_len);
        if (mcu_id == 0u) {
            co_printf("[MCU_TXN] unknown BLE cmd to MCU id map: 0x%04X\\r\\n",
                      (unsigned)id);
            /* 映射失败（未定义的指令），直接回失败 */
            BleFunc_SendResultToConidx(conidx, reply_cmd, 0x01);
            return;
        }
    }

    /* 2.1 统一规则：发�?MCU �?payload 不包�?Time6 */
    uint16_t payload_len_before_strip = payload_len;
    BleFunc_StripTime6_ForMcu(&payload, &payload_len);
    /*
     * 联调辅助�?x63FD(智能开�? 下发�?MCU 后应只剩 2B：control + controlType�?
     * 例如你举例：关闭充电显示�?-> control=0x00 controlType=0x04 => data 应为
     * 00 04�?
     */
    if (id == 0x63FDu) {
        uint8_t b0 = (payload != NULL && payload_len > 0u) ? payload[0] : 0u;
        uint8_t b1 = (payload != NULL && payload_len > 1u) ? payload[1] : 0u;
        co_printf("[MCU_TXN] 0x63FD strip %u->%u data=%02X %02X%s\\r\\n",
                  (unsigned)payload_len_before_strip, (unsigned)payload_len,
                  (unsigned)b0, (unsigned)b1,
                  (payload_len == 2u) ? "" : " (warn:expect 2B)");
    }

    /* 3. 发送串口数据给 MCU */
    uint8_t ok = SocMcu_Frame_Send(SOC_MCU_SYNC_SOC_TO_MCU, feature, mcu_id,
                                   payload, payload_len);
    if (!ok) {
        co_printf("[MCU_TXN] uart send failed\\r\\n");
        BleFunc_SendResultToConidx(conidx, reply_cmd, 0x01);
        return;
    }

    /* 4. 记录事务上下文并启动超时计时�?*/
    s_mcu_pending.active    = true;
    s_mcu_pending.conidx    = conidx;
    s_mcu_pending.reply_cmd = reply_cmd;
    /*
     * feature：发送请求时使用的通道（通常 FF01�?
     * resp_feature：匹配回包时使用的通道（通常 FF02）；同时兼容少量 MCU
     * 固件会回�?feature 的情况�?
     */
    s_mcu_pending.req_feature  = feature;
    s_mcu_pending.resp_feature = BLEFUNC_MCU_FEATURE_RESP;
    s_mcu_pending.id = mcu_id; /* 注意：这里记录的是发�?MCU �?ID，用于匹配回�?*/
    os_timer_start(&s_mcu_pending.timer, BLEFUNC_MCU_TIMEOUT_MS, false);
}

/**
 * @brief [DEBUG-SIM 专用] 仅通过串口下发�?MCU，不挂起等待、不影响当前 BLE
 * 的模拟回包�?
 *
 * @why
 * - 你要求在 ENABLE_NFC_ADD_SIMULATION �?#else 分支里“保留原有模拟成功逻辑”，
 *   但同时“也要把指令直接用串口接口发�?MCU”�?
 * - 生产链路仍建议使�?BleFunc_McuTxn_Start()（带超时/匹配/回包）�?
 */
static void BleFunc_McuUart_SendOnly(uint16_t feature, uint16_t ble_cmd,
                                     const uint8_t *payload,
                                     uint16_t payload_len)
{
    uint16_t mcu_id = ble_cmd;
    if (((ble_cmd & 0x00FFu) == 0x00FEu) || ((ble_cmd & 0x00FFu) == 0x00FDu)) {
        mcu_id = BleFunc_MapBleCmdToMcuId(ble_cmd, payload, payload_len);
        if (mcu_id == 0u) {
            co_printf("[DEBUG_SIM] uart skip: map ble_cmd=0x%04X failed\\r\\n",
                      (unsigned)ble_cmd);
            return;
        }
    }

    /* 统一规则：发�?MCU �?payload 不包�?Time6 */
    uint16_t payload_len_before_strip = payload_len;
    BleFunc_StripTime6_ForMcu(&payload, &payload_len);
    if (ble_cmd == 0x63FDu) {
        uint8_t b0 = (payload != NULL && payload_len > 0u) ? payload[0] : 0u;
        uint8_t b1 = (payload != NULL && payload_len > 1u) ? payload[1] : 0u;
        co_printf("[DEBUG_SIM] 0x63FD strip %u->%u data=%02X %02X%s\\r\\n",
                  (unsigned)payload_len_before_strip, (unsigned)payload_len,
                  (unsigned)b0, (unsigned)b1,
                  (payload_len == 2u) ? "" : " (warn:expect 2B)");
    }

    uint8_t ok = SocMcu_Frame_Send(SOC_MCU_SYNC_SOC_TO_MCU, feature, mcu_id,
                                   payload, payload_len);
    co_printf(
        "[DEBUG_SIM] uart send: ble_cmd=0x%04X -> mcu_id=0x%04X ok=%u\\r\\n",
        (unsigned)ble_cmd, (unsigned)mcu_id, (unsigned)ok);
}

/* 6.11 vehicle status sync (0x66FD) */
#define BLEFUNC_MCU_ID_VEHICLE_STATUS_PREADY (0x0213u)
#define BLEFUNC_MCU_ID_VEHICLE_SPEED         (0x0211u)
#define BLEFUNC_MCU_ID_VEHICLE_GEAR          (0x0202u)
#define BLEFUNC_MCU_ID_PARAM_SYNC_64FD       (0x0208u)

#define BLEFUNC_VS_VALID_PREADY              (0x01u)
#define BLEFUNC_VS_VALID_GEAR                (0x02u)
#define BLEFUNC_VS_VALID_SPEED               (0x04u)
#define BLEFUNC_PARAM_SYNC_64FD_LEN          (43u)
static uint8_t s_vs_acc         = 0u;
static uint8_t s_vs_pready      = 0u;
static uint8_t s_vs_gear        = 0u;
static uint16_t s_vs_speed      = 0u;
static uint8_t s_vs_valid_mask  = 0u;
static uint8_t s_vs_sent_once   = 0u;
static uint8_t s_vs_last_status = 0u;
static uint16_t s_vs_last_speed = 0u;

static uint8_t BleFunc_BuildVehicleStatusByte(void)
{
    uint8_t status = 0u;
    status |= (uint8_t)((s_vs_acc & 0x01u) << 7);
    status |= (uint8_t)((s_vs_pready & 0x03u) << 5);
    status |= (uint8_t)((s_vs_gear & 0x03u) << 2);
    return status;
}

static void BleFunc_ParamSync_MaybeNotify(void)
{
    const uint8_t need =
        (uint8_t)(BLEFUNC_VS_VALID_PREADY | BLEFUNC_VS_VALID_GEAR |
                  BLEFUNC_VS_VALID_SPEED);
    if ((s_vs_valid_mask & need) != need) { return; }
    uint8_t status = BleFunc_BuildVehicleStatusByte();
    if (!s_vs_sent_once || status != s_vs_last_status ||
        s_vs_speed != s_vs_last_speed) {
        s_vs_sent_once   = 1u;
        s_vs_last_status = status;
        s_vs_last_speed  = s_vs_speed;
        ParamSync_NotifyChange(status, s_vs_speed);
    }
}

static void BleFunc_ParamSync_RequestVehicleStatusFromMcu(void)
{
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                             (uint16_t)BLEFUNC_MCU_ID_VEHICLE_STATUS_PREADY,
                             NULL, 0u);
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                             (uint16_t)BLEFUNC_MCU_ID_VEHICLE_SPEED, NULL, 0u);
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                             (uint16_t)BLEFUNC_MCU_ID_VEHICLE_GEAR, NULL, 0u);
}

static void BleFunc_ParamSync_Request64FDFromMcu(void)
{
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                             (uint16_t)BLEFUNC_MCU_ID_PARAM_SYNC_64FD, NULL,
                             0u);
}

static bool BleFunc_PrintTime6(const uint8_t *time6)
{
    char time_str[13];
    if (proto_time6_bcd_to_str12(time6, time_str)) {
        co_printf("    time_bcd=%s\\r\\n", time_str);
        return true;
    }
    co_printf("    time_bcd=<invalid bcd>\\r\\n");
    return false;
}

static const char *BleFunc_McuCmdName(uint16_t id)
{
    if (id == BLEFUNC_MCU_ID_VEHICLE_STATUS_PREADY)
        return "BLEFUNC_MCU_ID_VEHICLE_STATUS_PREADY";
    if (id == BLEFUNC_MCU_ID_VEHICLE_SPEED)
        return "BLEFUNC_MCU_ID_VEHICLE_SPEED";
    if (id == BLEFUNC_MCU_ID_VEHICLE_GEAR) return "BLEFUNC_MCU_ID_VEHICLE_GEAR";
    if (id == BLEFUNC_MCU_ID_PARAM_SYNC_64FD)
        return "BLEFUNC_MCU_ID_PARAM_SYNC_64FD";
    switch (id) {
        case CMD_CONNECT:
            return "CMD_CONNECT";
        case CMD_BLE_FACTORY_RESET:
            return "CMD_BLE_FACTORY_RESET";
        case CMD_BLE_CANCEL_PAIRING:
            return "CMD_BLE_CANCEL_PAIRING";
        case CMD_ADD_NFC_KEY:
            return "CMD_ADD_NFC_KEY";
        case CMD_DELETE_NFC_KEY:
            return "CMD_DELETE_NFC_KEY";
        case CMD_FIND_NFC_KEY:
            return "CMD_FIND_NFC_KEY";
        case CMD_NFC_UNLOCK:
            return "CMD_NFC_UNLOCK";
        case CMD_NFC_LOCK:
            return "CMD_NFC_LOCK";
        case CMD_NFC_SWITCH:
            return "CMD_NFC_SWITCH";
        case CMD_OIL_VEHICLE_UNPREVENTION:
            return "CMD_OIL_VEHICLE_UNPREVENTION";
        case CMD_OIL_VEHICLE_PREVENTION:
            return "CMD_OIL_VEHICLE_PREVENTION";
        case CMD_OIL_VEHICLE_FIND_STATUS:
            return "CMD_OIL_VEHICLE_FIND_STATUS";
        case CMD_OIL_VEHICLE_UNLOCK_TRUNK:
            return "CMD_OIL_VEHICLE_UNLOCK_TRUNK";
        case CMD_OIL_VEHICLE_LOCK_TRUNK:
            return "CMD_OIL_VEHICLE_LOCK_TRUNK";
        case CMD_VEHICLE_UNLOCK_SEAT:
            return "CMD_VEHICLE_UNLOCK_SEAT";
        case CMD_VEHICLE_LOCK_SEAT:
            return "CMD_VEHICLE_LOCK_SEAT";
        case CMD_VEHICLE_MUTE_SETTING:
            return "CMD_VEHICLE_MUTE_SETTING";
        case CMD_VEHICLE_UNLOCK_MIDDLE_BOX:
            return "CMD_VEHICLE_UNLOCK_MIDDLE_BOX";
        case CMD_VEHICLE_LOCK_MIDDLE_BOX:
            return "CMD_VEHICLE_LOCK_MIDDLE_BOX";
        case CMD_VEHICLE_EMERGENCY_MODE_LOCK:
            return "CMD_VEHICLE_EMERGENCY_MODE_LOCK";
        case CMD_VEHICLE_EMERGENCY_MODE_UNLOCK:
            return "CMD_VEHICLE_EMERGENCY_MODE_UNLOCK";
        case CMD_BLE_MAC_READ:
            return "CMD_BLE_MAC_READ";
        case CMD_SINGLE_CONTROL_UNLOCK:
            return "CMD_SINGLE_CONTROL_UNLOCK";
        case CMD_Assistive_trolley_mode_on:
            return "CMD_Assistive_trolley_mode_on";
        case CMD_Assistive_trolley_mode_off:
            return "CMD_Assistive_trolley_mode_off";
        case CMD_Assistive_trolley_mode_default:
            return "CMD_Assistive_trolley_mode_default";
        case CMD_Delayed_headlight_on:
            return "CMD_Delayed_headlight_on";
        case CMD_Delayed_headlight_off:
            return "CMD_Delayed_headlight_off";
        case CMD_Delayed_headlight_default:
            return "CMD_Delayed_headlight_default";
        case CMD_Delayed_headlight_time_set:
            return "CMD_Delayed_headlight_time_set";
        case CMD_Charging_power_set:
            return "CMD_Charging_power_set";
        case CMD_AUTO_P_GEAR_on:
            return "CMD_AUTO_P_GEAR_on";
        case CMD_AUTO_P_GEAR_off:
            return "CMD_AUTO_P_GEAR_off";
        case CMD_AUTO_P_GEAR_default:
            return "CMD_AUTO_P_GEAR_default";
        case CMD_AUTO_P_GEAR_time_set:
            return "CMD_AUTO_P_GEAR_time_set";
        case CMD_Chord_horn_on:
            return "CMD_Chord_horn_on";
        case CMD_Chord_horn_off:
            return "CMD_Chord_horn_off";
        case CMD_Chord_horn_default:
            return "CMD_Chord_horn_default";
        case CMD_Chord_horn_type_set:
            return "CMD_Chord_horn_type_set";
        case CMD_Chord_horn_volume_set:
            return "CMD_Chord_horn_volume_set";
        case CMD_Three_shooting_lamp_on:
            return "CMD_Three_shooting_lamp_on";
        case CMD_Three_shooting_lamp_off:
            return "CMD_Three_shooting_lamp_off";
        case CMD_Three_shooting_lamp_default:
            return "CMD_Three_shooting_lamp_default";
        case CMD_Three_shooting_lamp_mode_set:
            return "CMD_Three_shooting_lamp_mode_set";
        case CMD_Assistive_reversing_gear_on:
            return "CMD_Assistive_reversing_gear_on";
        case CMD_Assistive_reversing_gear_off:
            return "CMD_Assistive_reversing_gear_off";
        case CMD_Assistive_reversing_gear_default:
            return "CMD_Assistive_reversing_gear_default";
        case CMD_Automatic_steering_reset_on:
            return "CMD_Automatic_steering_reset_on";
        case CMD_Automatic_steering_reset_off:
            return "CMD_Automatic_steering_reset_off";
        case CMD_Automatic_steering_reset_default:
            return "CMD_Automatic_steering_reset_default";
        case CMD_EBS_switch_on:
            return "CMD_EBS_switch_on";
        case CMD_Low_speed_gear_on:
            return "CMD_Low_speed_gear_on";
        case CMD_Low_speed_gear_off:
            return "CMD_Low_speed_gear_off";
        case CMD_Low_speed_gear_default:
            return "CMD_Low_speed_gear_default";
        case CMD_Low_speed_gear_speed_set:
            return "CMD_Low_speed_gear_speed_set";
        case CMD_Medium_speed_gear_on:
            return "CMD_Medium_speed_gear_on";
        case CMD_Medium_speed_gear_off:
            return "CMD_Medium_speed_gear_off";
        case CMD_Medium_speed_gear_default:
            return "CMD_Medium_speed_gear_default";
        case CMD_Medium_speed_gear_speed_set:
            return "CMD_Medium_speed_gear_speed_set";
        case CMD_High_speed_gear_on:
            return "CMD_High_speed_gear_on";
        case CMD_High_speed_gear_off:
            return "CMD_High_speed_gear_off";
        case CMD_High_speed_gear_default:
            return "CMD_High_speed_gear_default";
        case CMD_High_speed_gear_speed_set:
            return "CMD_High_speed_gear_speed_set";
        case CMD_Lost_mode_on:
            return "CMD_Lost_mode_on";
        case CMD_Lost_mode_off:
            return "CMD_Lost_mode_off";
        case CMD_TCS_switch_on:
            return "CMD_TCS_switch_on";
        case CMD_TCS_switch_off:
            return "CMD_TCS_switch_off";
        case CMD_Side_stand_switch_on:
            return "CMD_Side_stand_switch_on";
        case CMD_Side_stand_switch_off:
            return "CMD_Side_stand_switch_off";
        case CMD_Battery_type_set:
            return "CMD_Battery_type_set";
        case CMD_Battery_capacity_set:
            return "CMD_Battery_capacity_set";
        case CMD_KFA_2KGA_2MQA_data_sync:
            return "CMD_KFA_2KGA_2MQA_data_sync";
        case CMD_HDC_switch_on:
            return "CMD_HDC_switch_on";
        case CMD_HDC_switch_off:
            return "CMD_HDC_switch_off";
        case CMD_HHC_switch_on:
            return "CMD_HHC_switch_on";
        case CMD_HHC_switch_off:
            return "CMD_HHC_switch_off";
        case CMD_Starting_strength_set:
            return "CMD_Starting_strength_set";
        case CMD_SPORT_mode_on:
            return "CMD_SPORT_mode_on";
        case CMD_SPORT_mode_off:
            return "CMD_SPORT_mode_off";
        case CMD_SPORT_mode_default:
            return "CMD_SPORT_mode_default";
        case CMD_SPORT_mode_type_set:
            return "CMD_SPORT_mode_type_set";
        case CMD_ECO_mode_on:
            return "CMD_ECO_mode_on";
        case CMD_ECO_mode_off:
            return "CMD_ECO_mode_off";
        case CMD_ECO_mode_default:
            return "CMD_ECO_mode_default";
        case CMD_ECO_mode_type_set:
            return "CMD_ECO_mode_type_set";
        case CMD_POWER_RECOVER_ON:
            return "CMD_POWER_RECOVER_ON";
        case CMD_POWER_RECOVER_OFF:
            return "CMD_POWER_RECOVER_OFF";
        case CMD_ABS_ON_OFF:
            return "CMD_ABS_ON_OFF";
        case CMD_POWER_LED:
            return "CMD_POWER_LED";
        case CMD_SADDLE_ON_OFF:
            return "CMD_SADDLE_ON_OFF";
        case CMD_Position_light_on_off:
            return "CMD_Position_light_on_off";
        case CMD_Radar_switch_on:
            return "CMD_Radar_switch_on";
        case CMD_Radar_switch_off:
            return "CMD_Radar_switch_off";
        case CMD_Radar_switch_default:
            return "CMD_Radar_switch_default";
        case CMD_Radar_sensitivity_set:
            return "CMD_Radar_sensitivity_set";
        case CMD_Tire_pressure_monitoring_get:
            return "CMD_Tire_pressure_monitoring_get";
        case CMD_BLE_RSSI_READ:
            return "CMD_BLE_RSSI_READ";
        case CMD_BLE_RSSI_RANGE_SET:
            return "CMD_BLE_RSSI_RANGE_SET";
        case CMD_CHARGE_DISPLAY_ON:
            return "CMD_CHARGE_DISPLAY_ON";
        case CMD_CHARGE_DISPLAY_OFF:
            return "CMD_CHARGE_DISPLAY_OFF";
        default:
            return "UNKNOWN";
    }
}
static void BleFunc_LogMcuUartFrame(uint16_t feature, uint16_t id,
                                    const uint8_t *data, uint16_t data_len)
{
    const char *name = BleFunc_McuCmdName(id);
    co_printf("[MCU_UART] feature=0x%04X id=0x%04X (%s) len=%u\\r\\n",
              (unsigned)feature, (unsigned)id, name, (unsigned)data_len);
    if (data == NULL || data_len == 0u) {
        co_printf("    data: <empty>\\r\\n");
        return;
    }
    co_printf("    data:");
    for (uint16_t i = 0; i < data_len; i++) { co_printf(" %02X", data[i]); }
    co_printf("\\r\\n");
}
/*********************************************************************
 * @brief 进行回复给蓝牙app
 * @param reply_cmd {placeholder}
 * @param result_code {placeholder}
 */
static void BleFunc_SendResultToRx(uint16_t reply_cmd, uint8_t result_code)
{
    uint8_t conidx = Protocol_Get_Rx_Conidx();
    uint8_t resp_payload[1];
    resp_payload[0] = result_code;
    (void)Protocol_Send_Unicast(conidx, reply_cmd, resp_payload,
                                (uint16_t)sizeof(resp_payload));
}

/*********************************************************************
 * @brief 发送默认回复（通常用于未实现的功能或默认失败情况）
 * @param reply_cmd 回复的命令ID
 */
#if defined(__CC_ARM)
#pragma diag_suppress 177 /* function was declared but never referenced */
#endif
static void BleFunc_SendResultToRx_Default(uint16_t reply_cmd)
{
    /* 默认回失�?(0x01) */
    BleFunc_SendResultToRx(reply_cmd, 0x01);
}

static void BleFunc_SendResultToRx_WithData(uint16_t reply_cmd,
                                            const uint8_t *data, uint16_t len)
{
    uint8_t conidx = Protocol_Get_Rx_Conidx();
    (void)Protocol_Send_Unicast(conidx, reply_cmd, data, len);
}
/*********************************************************************
 * @brief 确保当前连接已鉴�?
 * @param reply_cmd {placeholder}
 * @retval {placeholder}
 * @retval {placeholder}
 */
static bool BleFunc_EnsureAuthed(uint16_t reply_cmd)
{
    uint8_t conidx = Protocol_Get_Rx_Conidx();
    if (Protocol_Auth_IsOk(conidx)) { return true; }
    co_printf("    auth not ok, reject cmd\\r\\n");
    BleFunc_SendResultToRx_Default(reply_cmd);
    return false;
}
/*********************************************************************
 * @brief 打印 Payload 内容（十六进制）
 * @param payload {placeholder}
 * @param len {placeholder}
 */
static void BleFunc_DumpPayload(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len == 0) {
        co_printf("    payload: <empty>\\r\\n");
        return;
    }
    co_printf("    payload(%d):\\r\\n", (int)len);
    for (uint8_t i = 0; i < len; i++) { co_printf(" %02X\\r\\n", payload[i]); }
    co_printf("\\r\\n");
}

/* ====== 5.1 Connect(0x01FE) 辅助函数 ======
 * 说明：这些是“工具函数”，放在文件级以满足标准 C（禁止函数嵌套定义）�?
 */
static uint8_t BleFunc_ToUpper(uint8_t c)
{
    if (c >= 'a' && c <= 'z') { return (uint8_t)(c - 'a' + 'A'); }
    return c;
}

static bool BleFunc_IsHexAscii(uint8_t c)
{
    c = BleFunc_ToUpper(c);
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

static void BleFunc_DumpHexInline(const uint8_t *data, uint16_t dump_len)
{
    if (data == NULL || dump_len == 0) {
        co_printf("<empty>\\r\\n");
        return;
    }
    for (uint16_t i = 0; i < dump_len; i++) {
        co_printf("%02X\\r\\n", data[i]);
    }
}

static bool BleFunc_IsAllZero(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0u) { return true; }
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] != 0u) { return false; }
    }
    return true;
}

void BleFunc_OnMcuUartFrame(uint16_t sync, uint16_t feature, uint16_t id,
                            const uint8_t *data, uint16_t data_len,
                            uint8_t crc_ok)
{
    if (!crc_ok) {
        co_printf("[MCU_TXN] drop frame: crc bad\\r\\n");
        return;
    }
    if (sync != SOC_MCU_SYNC_MCU_TO_SOC) { return; }

    /* 1) 如果是正在等待的 MCU 应答，则按事务回包到 APP（reply_cmd�?*/
    if (s_mcu_pending.active && id == s_mcu_pending.id &&
        (feature == s_mcu_pending.resp_feature ||
         feature == s_mcu_pending.req_feature)) {
        /* 先停表，后续如果需要链式继续，会重新启�?*/
        os_timer_stop(&s_mcu_pending.timer);
        s_mcu_pending.active = false;

        co_printf("[MCU_TXN] matched: conidx=%u feature=0x%04X id=0x%04X "
                  "reply=0x%04X data_len=%u\\r\\n",
                  (unsigned)s_mcu_pending.conidx, (unsigned)feature,
                  (unsigned)id, (unsigned)s_mcu_pending.reply_cmd,
                  (unsigned)data_len);

        /* ===== 链式事务�?x06FD 和弦喇叭（先音源0x59后音�?x60，均�?data
         * 1B�?===== */
        if (s_mcu_pending.chain_kind == BLEFUNC_MCU_CHAIN_CHORD_HORN_06FD &&
            s_mcu_pending.chain_step == 0u) {
            uint8_t result = 0x00u;
            if (data != NULL && data_len >= 1u) { result = data[0]; }
            /* �?1 步失败：直接回复 APP（保持和现有事务一致，优先透传 MCU
             * payload�?*/
            if (result != 0x00u) {
                s_mcu_pending.chain_kind = BLEFUNC_MCU_CHAIN_NONE;
                s_mcu_pending.chain_step = 0u;
                if (data_len == 0u || data == NULL) {
                    BleFunc_SendResultToConidx(s_mcu_pending.conidx,
                                               s_mcu_pending.reply_cmd, 0x01);
                    return;
                }
                (void)Protocol_Send_Unicast(s_mcu_pending.conidx,
                                            s_mcu_pending.reply_cmd, data,
                                            data_len);
                return;
            }

            /* �?1 步成功：触发�?2 步（音量），等待其回包后再回�?BLE */
            uint8_t volume           = s_mcu_pending.chain_u8_0;
            s_mcu_pending.chain_step = 1u;
            co_printf(
                "[MCU_TXN] chain(06FD) step1 ok, send step2 volume=%u\\r\\n",
                (unsigned)volume);
            BleFunc_McuTxn_Start(s_mcu_pending.conidx, s_mcu_pending.reply_cmd,
                                 s_mcu_pending.req_feature,
                                 (uint16_t)CMD_Chord_horn_volume_set, &volume,
                                 1u);
            return;
        }

        /* ===== 链式事务：低/�?高速档位（�?on，再 speed_set；speed_set
         * data=1B�?===== */
        if (s_mcu_pending.chain_kind == BLEFUNC_MCU_CHAIN_GEAR_SET_FD &&
            s_mcu_pending.chain_step == 0u) {
            uint8_t result = 0x00u;
            if (data != NULL && data_len >= 1u) { result = data[0]; }
            /* �?1 步失败：直接回复 APP（优先透传 MCU payload�?*/
            if (result != 0x00u) {
                s_mcu_pending.chain_kind  = BLEFUNC_MCU_CHAIN_NONE;
                s_mcu_pending.chain_step  = 0u;
                s_mcu_pending.chain_u16_0 = 0u;
                if (data_len == 0u || data == NULL) {
                    BleFunc_SendResultToConidx(s_mcu_pending.conidx,
                                               s_mcu_pending.reply_cmd, 0x01);
                    return;
                }
                (void)Protocol_Send_Unicast(s_mcu_pending.conidx,
                                            s_mcu_pending.reply_cmd, data,
                                            data_len);
                return;
            }

            /* �?1 步成功：触发�?2 步（speed_set），等待其回包后再回�?BLE */
            uint8_t speed            = s_mcu_pending.chain_u8_0;
            uint16_t step2_id        = s_mcu_pending.chain_u16_0;
            s_mcu_pending.chain_step = 1u;
            co_printf("[MCU_TXN] chain(GEAR) step1 ok, send step2 id=0x%04X "
                      "speed=%u\\r\\n",
                      (unsigned)step2_id, (unsigned)speed);
            BleFunc_McuTxn_Start(s_mcu_pending.conidx, s_mcu_pending.reply_cmd,
                                 s_mcu_pending.req_feature, step2_id, &speed,
                                 1u);
            return;
        }

        /* =====
         * 链式事务：雷达开�?灵敏度（先开关，�?sensitivity_set；data=1B�?=====
         */
        if (s_mcu_pending.chain_kind == BLEFUNC_MCU_CHAIN_RADAR_SET_FD &&
            s_mcu_pending.chain_step == 0u) {
            uint8_t result = 0x00u;
            if (data != NULL && data_len >= 1u) { result = data[0]; }
            if (result != 0x00u) {
                s_mcu_pending.chain_kind  = BLEFUNC_MCU_CHAIN_NONE;
                s_mcu_pending.chain_step  = 0u;
                s_mcu_pending.chain_u16_0 = 0u;
                if (data_len == 0u || data == NULL) {
                    BleFunc_SendResultToConidx(s_mcu_pending.conidx,
                                               s_mcu_pending.reply_cmd, 0x01);
                    return;
                }
                (void)Protocol_Send_Unicast(s_mcu_pending.conidx,
                                            s_mcu_pending.reply_cmd, data,
                                            data_len);
                return;
            }

            uint8_t sensitivity      = s_mcu_pending.chain_u8_0;
            uint16_t step2_id        = s_mcu_pending.chain_u16_0;
            s_mcu_pending.chain_step = 1u;
            co_printf("[MCU_TXN] chain(RADAR) step1 ok, send step2 id=0x%04X "
                      "sensitivity=%u\\r\\n",
                      (unsigned)step2_id, (unsigned)sensitivity);
            BleFunc_McuTxn_Start(s_mcu_pending.conidx, s_mcu_pending.reply_cmd,
                                 s_mcu_pending.req_feature, step2_id,
                                 &sensitivity, 1u);
            return;
        }

        /* 非链式，或链式最后一步：回包�?APP 并清理链式状�?*/
        s_mcu_pending.chain_kind  = BLEFUNC_MCU_CHAIN_NONE;
        s_mcu_pending.chain_step  = 0u;
        s_mcu_pending.chain_u16_0 = 0u;
        if (data_len == 0u || data == NULL) {
            BleFunc_SendResultToConidx(s_mcu_pending.conidx,
                                       s_mcu_pending.reply_cmd, 0x00);
            return;
        }
        /* 约定：MCU payload = ResultCode(1) + 可选数据；BLE 侧原样透传 */
        (void)Protocol_Send_Unicast(s_mcu_pending.conidx,
                                    s_mcu_pending.reply_cmd, data, data_len);
        return;
    }

    /* 2) 非事务类回包：认为是 MCU 主动上报，转发到已鉴�?APP（Notify�?*/
    /* 6.11 vehicle status sync (0x66FD) from MCU */
    if ((feature == (uint16_t)BLEFUNC_MCU_FEATURE_RESP ||
         feature == (uint16_t)BLEFUNC_MCU_FEATURE_REQ)) {
        if (id == (uint16_t)BLEFUNC_MCU_ID_VEHICLE_STATUS_PREADY &&
            data != NULL && data_len >= 1u) {
            uint8_t v   = data[0];
            s_vs_acc    = (uint8_t)((v >> 7) & 0x01u);
            s_vs_pready = (uint8_t)(v & 0x03u);
            s_vs_valid_mask |= BLEFUNC_VS_VALID_PREADY;
            BleFunc_ParamSync_MaybeNotify();
            return;
        }
        if (id == (uint16_t)BLEFUNC_MCU_ID_VEHICLE_GEAR && data != NULL &&
            data_len >= 1u) {
            uint8_t v = data[0];
            s_vs_gear = (uint8_t)((v >> 2) & 0x03u);
            s_vs_valid_mask |= BLEFUNC_VS_VALID_GEAR;
            BleFunc_ParamSync_MaybeNotify();
            return;
        }
        if (id == (uint16_t)BLEFUNC_MCU_ID_VEHICLE_SPEED && data != NULL &&
            data_len >= 2u) {
            s_vs_speed = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
            s_vs_valid_mask |= BLEFUNC_VS_VALID_SPEED;
            BleFunc_ParamSync_MaybeNotify();
            return;
        }
        if (id == (uint16_t)BLEFUNC_MCU_ID_PARAM_SYNC_64FD && data != NULL &&
            data_len >= BLEFUNC_PARAM_SYNC_64FD_LEN) {
            ParamSync_OnMcuSync64FD(data, data_len);
            return;
        }
    }
    /* TPMS 0x0117：若 MCU 现阶段回�?0，则用模拟数据兜底，先把 App 链路跑�?*/
    if (BLEFUNC_TPMS_SIM_ENABLE &&
        feature == (uint16_t)BLEFUNC_MCU_FEATURE_RESP &&
        id == (uint16_t)CMD_Tire_pressure_monitoring_get &&
        data_len == (uint16_t)sizeof(s_tpms_sim_payload_0117) &&
        BleFunc_IsAllZero(data, data_len)) {
        co_printf("[TPMS_SIM] replace all-zero payload: id=0x%04X len=%u\\r\\n",
                  (unsigned)id, (unsigned)data_len);
        BleFunc_PushMcuFrameToAuthedApp(
            feature, id, s_tpms_sim_payload_0117,
            (uint16_t)sizeof(s_tpms_sim_payload_0117));
        return;
    }

    BleFunc_PushMcuFrameToAuthedApp(feature, id, data, data_len);
}
/*********************************************************************
 * @brief  5.1 连接指令（APP -> 设备，鉴权登录）
 * @param cmd {placeholder}
 * @param payload {placeholder}
 * @param len {placeholder}
 * @note 该指令用�?APP 端登录鉴权，payload 包含时间戳、Token 等信息�?
 */
void BleFunc_FE_Connect(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    (void)cmd;
    co_printf("  -> Connect\\r\\n");

    /*
     * 文档定义：Time(6) + Token(32=MD5(T-BOXKEY)的HEX字符�? + mobileSystem(1)
     * 兼容性保留：若对端发�?16 字节原始
     * MD5，则�?Time(6)+Token(16)+mobileSystem(1) 解析�?
     */

    // 0x01FE 数据段定义：Time(6) + Token(32) + mobileSystem(1) = 39
    // 兼容性修改：如果长度 > 39，可能是因为 AES 加密填充（Padding），只取�?39
    // 字节
    if (payload == NULL || len < 39) {
        co_printf("    invalid payload len=%d (expect >= 39)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        // 校验失败，断开
        Protocol_Disconnect(Protocol_Get_Rx_Conidx());
        return;
    }

    const uint8_t *time6     = &payload[0];
    const uint8_t *token_ptr = &payload[6];
    uint8_t token_len        = 0;
    uint8_t mobileSystem     = 0xFF;

    bool token32_is_ascii_hex = true;
    for (uint8_t i = 0; i < 32; i++) {
        if (!BleFunc_IsHexAscii(token_ptr[i])) {
            token32_is_ascii_hex = false;
            break;
        }
    }

    /* 优先按�?2字节ASCII HEX”解析；否则回退为�?6字节原始MD5”解�?*/
    if (token32_is_ascii_hex) {
        token_len    = 32;
        mobileSystem = payload[38];
    } else {
        /* 16B Token: Time(6) + Token(16) + mobileSystem(1) = 23 */
        token_len    = 16;
        mobileSystem = payload[22];
    }

    /* mobileSystem: 1=android, 2=ios, 3=other（按文档�?*/
    co_printf("    mobileSystem=0x%02X\\r\\n", mobileSystem);
    if (!(mobileSystem == 1 || mobileSystem == 2 || mobileSystem == 3)) {
        co_printf("    mobileSystem invalid (expect 1/2/3)\\r\\n");
    }

    /* Token 打印：若�?ASCII HEX，则按字符串更直观；否则�?HEX 输出 */
    if (token_len == 32 && token32_is_ascii_hex) {
        char rx_token_str[33];
        memcpy(rx_token_str, token_ptr, 32);
        rx_token_str[32] = '\0';
        co_printf("    Rx Token(32 str): %s\\r\\n", rx_token_str);
    } else {
        co_printf("    Rx Token(%d hex): \\r\\n", token_len);
        BleFunc_DumpHexInline(token_ptr, token_len);
        co_printf("\\r\\n");
    }

    char time_str[13];
    if (proto_time6_bcd_to_str12(time6, time_str)) {
        co_printf("    time_bcd=%s\\r\\n", time_str);
    } else {
        co_printf("    time_bcd=invalid\\r\\n");
        /* [Debug] 解析失败时，打印�?16 个字节，检查解密后的数�?*/
        co_printf("    Decrypted Hex(16): \\r\\n");
        for (int i = 0; i < 16 && i < len; i++)
            co_printf("%02X \\r\\n", payload[i]);
        co_printf("\\r\\n");
    }

    /* --- MD5 Token Verification --- */
    uint8_t target_digest[16];
    Algo_MD5_Calc(TBOX_KEY, sizeof(TBOX_KEY), target_digest);

    // Convert Digest to Hex String (upper case)
    char calculated_token[33];
    const char hex_map[] = "0123456789ABCDEF";
    for (int i = 0; i < 16; i++) {
        calculated_token[i * 2]     = hex_map[(target_digest[i] >> 4) & 0x0F];
        calculated_token[i * 2 + 1] = hex_map[target_digest[i] & 0x0F];
    }
    calculated_token[32] = '\0'; // Null terminator

    /* 默认期望 Token：用设备�?TBOX_KEY 计算出来�?MD5 HEX */
    char expected_token_up[33];
    memcpy(expected_token_up, calculated_token, 33);

    /* 如果配置了覆盖宏，则以覆盖值为准（忽略大小写） */
    if (sizeof(TBOX_TOKEN_MD5_ASCII) > 1u) {
        for (uint8_t i = 0; i < 32; i++) {
            expected_token_up[i] =
                (char)BleFunc_ToUpper((uint8_t)TBOX_TOKEN_MD5_ASCII[i]);
        }
        expected_token_up[32] = '\0';
        co_printf("    Local Expected Token(override): %s\\r\\n",
                  expected_token_up);
    } else {
        co_printf("    Local Calc Token: %s\\r\\n", expected_token_up);
    }

    /* Compare received token with calculated token (兼容两种格式) */
    bool auth_ok = false;
#if BLEFUNC_DEV_ACCEPT_ANY_TOKEN
    /* 开发阶段：只要 0x01FE 解析成功，就认为鉴权通过（不比较 Token，不断连�?*/
    auth_ok = true;
#else
    if (token_len == 32) {
        /* 32 字节 ASCII HEX：忽略大小写 */
        uint8_t rx_up[32];
        for (uint8_t i = 0; i < 32; i++)
            rx_up[i] = BleFunc_ToUpper(token_ptr[i]);
        if (memcmp(rx_up, expected_token_up, 32) == 0) { auth_ok = true; }
    } else if (token_len == 16) {
        /* 16 字节原始 MD5 */
        if (memcmp(token_ptr, target_digest, 16) == 0) { auth_ok = true; }
    }
#endif

    if (auth_ok) {
        co_printf("    Auth Success!\\r\\n");
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        Protocol_Auth_Set(conidx, true);
        /*回传鉴权成功*/
        Protocol_Auth_SendResult(conidx, true);
        /*
         * 6.10/6.11：鉴权成功后主动同步一次参数到 APP�?
         * 为什么放在这里：
         * -
         * 文档要求“每次蓝牙连接上后同步一次”，而工程里业务推送只对已鉴权连接开放�?
         * - 放在鉴权成功点，能避免未登录连接收到业务帧导�?APP 误解析�?
         */
        ParamSync_OnBleAuthed(conidx);
        BleFunc_ParamSync_Request64FDFromMcu();
        BleFunc_ParamSync_RequestVehicleStatusFromMcu();
    } else {
        co_printf("    Auth Failed!\\r\\n");

        uint8_t conidx = Protocol_Get_Rx_Conidx();
        Protocol_Auth_Set(conidx, false);
        Protocol_Auth_SendResult(conidx, false);

        // 断开连接
        Protocol_Disconnect(conidx);
    }
}

/*********************************************************************
 * @brief 5.3 设防/撤防设置指令（APP -> 设备，开关电门锁�?
 * @details
 * - Request Cmd: 0x03FE
 * - Payload: Time(6) + DefendStatus(1)
 *   - DefendStatus: 0x00 手动撤防�?x01 手动设防
 * - Response Cmd: 0x0301
 * - Response Payload: ResultCode(1)
 *   - 0x00 成功�?x01 失败
 * @note
 * - 该指令通常会涉�?IO/继电器动作（电门锁）�?
 * - 当前实现做协议解析与回包框架；硬件动作未接入时默认回失败，避免误控�?
 */
void BleFunc_FE_Defences(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Defences (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd); // 0x0301

    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /* 0x03FE: Time(6) + DefendStatus(1) = 7 bytes */
    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (expect >= 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }

    const uint8_t *time6 = &payload[0];
    uint8_t defendStatus = payload[6];

    BleFunc_PrintTime6(time6);
    co_printf("    DefendStatus=0x%02X\\r\\n", defendStatus);
    BleFunc_DumpPayload(payload, len);

    uint8_t result = 0x01;
    /* 校验 DefendStatus 合法�?*/
    if (defendStatus != 0x00 && defendStatus != 0x01) {
        co_printf("    invalid DefendStatus\\r\\n");
        result = 0x01;
        BleFunc_SendResultToRx(reply_cmd, result);
    } else {
#if (!ENABLE_NFC_ADD_SIMULATION)
        /* [PRODUCTION Mode] 真实转发�?MCU */
        {
            uint8_t conidx = Protocol_Get_Rx_Conidx();
            /* MapBleCmdToMcuId �?0x03FE 转换�?0x30(撤防) / 0x31(设防) */
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)cmd, payload, (uint16_t)len);
        }
#else
        /* [DEBUG-ONLY] 模拟 MCU 回复成功 */
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload,
                                 (uint16_t)len);
        co_printf("[DEBUG_SIM] Defences(%s) Success (0x00)\\r\\n",
                  defendStatus ? "LOCK" : "UNLOCK");
        result = 0x00;
        BleFunc_SendResultToRx(reply_cmd, result);
#endif
    }
}

/**
 * @brief 5.4 蓝牙感应防盗开关设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x04FE
 * - Payload: Time(6) + InductionStatus(1)
 *   - 0x00 感应撤防�?x01 感应设防
 * - Response Cmd: 0x0401
 * - Response Payload: ResultCode(1)�?x00 成功�?x01 失败�?
 */
void BleFunc_FE_AntiTheft(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Anti-Theft (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /* 0x04FE: Time(6) + InductionStatus(1) = 7 bytes */
    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (expect >= 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }

    BleFunc_PrintTime6(&payload[0]);
    co_printf("    InductionStatus=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);

/* 下发�?MCU，并等待回包/超时再回 0x0401 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    /* [PRODUCTION Mode] 真实转发�?MCU */
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    /* [DEBUG-ONLY] 模拟 MCU 回复成功（同时镜像下�?MCU 便于联调对照�?*/
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] Anti-Theft Success (0x00)\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 5.5 手机消息通知（APP -> 设备�?
 * @details
 * - Request Cmd: 0x05FE
 * - Payload: Time(6) + MessageType(1)
 *   - 0x01 有电话待接听�?x02 有信息待查看
 * - Response Cmd: 0x0501
 * - Response Payload: ResultCode(1)
 * @note 该指令通常不涉及危险硬件动作，默认回成功以保证 APP 流程联调�?
 */
void BleFunc_FE_PhoneMessage(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Phone Message (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /* 0x05FE: Time(6) + MessageType(1) = 7 bytes */
    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (expect >= 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }

    BleFunc_PrintTime6(&payload[0]);
    co_printf("    MessageType=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);

/* 下发�?MCU，并等待回包/超时再回 0x0501 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    /* [PRODUCTION Mode] 真实转发�?MCU */
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    /* [DEBUG-ONLY] 模拟 MCU 回复成功（同时镜像下�?MCU 便于联调对照�?*/
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] Phone Message Success (0x00)\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 5.7 手机蓝牙信号强度范围设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x07FE
 * - Payload: Time(6) + min(2) + max(2)
 *   - 协议说明：因�?-1，MCU 收到�?min/max �?-1
 * - Response Cmd: 0x0701
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FE_RssiStrength(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> RSSI Strength (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /* 0x07FE: Time(6) + min(2) + max(2) = 10 bytes */
    if (payload == NULL || len < 10) {
        co_printf("    invalid payload len=%d (expect >= 10)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }

    BleFunc_PrintTime6(&payload[0]);
    /* 解析 min_raw �?max_raw 的值（APP 发送大端序�?*/
    uint16_t min_raw = ((uint16_t)payload[6] << 8) | (uint16_t)payload[7];
    uint16_t max_raw = ((uint16_t)payload[8] << 8) | (uint16_t)payload[9];
    int16_t min_dbm  = -(int16_t)min_raw; /* 协议因子 -1，转换为�?dBm */
    int16_t max_dbm  = -(int16_t)max_raw;
    co_printf("    min_raw=%u -> %d dBm\\r\\n", (unsigned)min_raw,
              (int)min_dbm);
    co_printf("    max_raw=%u -> %d dBm\\r\\n", (unsigned)max_raw,
              (int)max_dbm);

    BleFunc_DumpPayload(payload, len);

/* 下发�?MCU，并等待回包/超时再回 0x0701 */
#if ((!BLEFUNC_FE_AUTO_REPLY_ENABLE) || (!BLEFUNC_FE_AUTO_REPLY_RSSI_STRENGTH))
    /* [PRODUCTION Mode] 真实转发�?MCU */
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    /* [DEBUG-ONLY] 自动成功回包（同时镜像下�?MCU 便于联调对照�?*/
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] RSSI Strength Success (0x00)\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 5.8 手机蓝牙寻车（APP -> 设备�?
 * @details
 * - Request Cmd: 0x08FE
 * - Payload: Time(6)
 * - Response Cmd: 0x0801
 * - Response Payload: ResultCode(1)
 * @note 通常会触发鸣�?双闪等动作，未接入硬件前默认回失败�?
 */
void BleFunc_FE_CarSearch(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Car Search (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /* 0x08FE: Time(6) = 6 bytes */
    if (payload == NULL || len < 6) {
        co_printf("    invalid payload len=%d (expect >= 6)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }

    BleFunc_PrintTime6(&payload[0]);
    BleFunc_DumpPayload(payload, len);

#if (!ENABLE_NFC_ADD_SIMULATION)
    /* [PRODUCTION Mode] 真实转发�?MCU，并等待 MCU 回包/超时后再�?0x0801 */
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    /* [DEBUG-ONLY] 模拟 MCU 回复成功 */
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] Car Search Success (0x00)\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 5.9 设备蓝牙恢复出厂设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x09FE
 * - Payload: Time(6)
 * - Response Cmd: 0x0901
 * - Response Payload: ResultCode(1)
 * @note
 * 属于破坏性操作，未接入持久化/清理流程前默认回失败。soc端不用做处理，让vcu端进行处理即可�?
 */
void BleFunc_FE_FactorySettings(uint16_t cmd, const uint8_t *payload,
                                uint8_t len)
{
    co_printf("  -> Factory Settings (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /* 0x09FE: Time(6) = 6 bytes */
    if (payload == NULL || len < 6) {
        co_printf("    invalid payload len=%d (expect >= 6)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    BleFunc_DumpPayload(payload, len);

/* TODO: 清理配对/参数/NFC 卡池等，并持久化 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    /* [PRODUCTION Mode] 转发 MCU 进行整车复位 */
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    /* [DEBUG-ONLY] 模拟 MCU 回复成功 */
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] Factory Settings Success (0x00)\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 5.10 取消手机当前配对信息（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0AFE
 * - Payload: Time(6)
 * - Response Cmd: 0x0A01
 * - Response Payload: ResultCode(1)
 *   - 0x00 成功�?x01 失败�?x02 操作失败：无效卡（协议文档给出的扩展含义�?
 */
void BleFunc_FE_BleUnpair(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> BLE Unpair (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /* 0x0AFE: Time(6) = 6 bytes */
    if (payload == NULL || len < 6) {
        co_printf("    invalid payload len=%d (expect >= 6)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    BleFunc_DumpPayload(payload, len);

    /*
     * 取消绑定/配对信息�?
     * - SDK bond manager 会把密钥/对端信息持久化到
     * flash（初始化�?ble_simple_peripheral.c �?gap_bond_manager_init�?
     * - 此处调用 gap_bond_manager_delete_all() 清空所�?bond 记录
     * - 先回包再断链，避免在断链后无法通过 notify �?0x0A01
     */
    BleFunc_SendResultToRx(reply_cmd, 0x00);

    uint8_t conidx = Protocol_Get_Rx_Conidx();
    Protocol_Auth_Clear(conidx);
    gap_bond_manager_delete_all();
    Protocol_Disconnect(conidx);
}

/**
 * @brief 5.11 手机增实�?NFC 卡（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0BFE
 * - Payload: Time(6) + add_control(1)
 *   - 0x01 开始添加；0x02 确定添加非加密卡�?x03 不添加非加密�?
 * - Response Cmd: 0x0B01
 * - Response Payload: ResultCode(1) + (uuid/key 可�?
 * @note
 * - 该功能涉�?NFC 识别/加密/卡池存储/超时流程�?
 * - 当前实现仅解析与回包框架；未接入卡池时默认回失败�?
 */
void BleFunc_FE_AddNfc(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Add NFC (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    add_control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);

/* 下发�?MCU，由 MCU 负责 NFC 交互逻辑（Reply 0x00/0x01/0x02/0x03/0x04 + UUID +
 * Key�?*/
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    /* [DEBUG-ONLY] 先保留模拟成功逻辑，同时镜像下�?MCU 便于联调对照 */
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] Add NFC Success (0x00)\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 5.12 手机删实�?NFC 卡（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0CFE
 * - Payload:
 *   - Time(6) + delete_control(1) + uuid(4) + key(6)   (delete_control=1)
 *   - Time(6) + delete_control(1)                     (delete_control=2/3)
 * - Response Cmd: 0x0C01
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FE_DeleteNfc(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Delete NFC (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (min 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }

    BleFunc_PrintTime6(&payload[0]);
    uint8_t control = payload[6];
    co_printf("    delete_control=%u\\r\\n", (unsigned)control);
    if (control == 1) {
        if (len != 17) {
            co_printf("    invalid len for control=1 (expect 17)\\r\\n");
            BleFunc_DumpPayload(payload, len);
            BleFunc_SendResultToRx(reply_cmd, 0x01);
            return;
        }
        co_printf("    uuid=%02X%02X%02X%02X\\r\\n", payload[7], payload[8],
                  payload[9], payload[10]);
        co_printf("    key=%02X %02X %02X %02X %02X %02X\\r\\n", payload[11],
                  payload[12], payload[13], payload[14], payload[15],
                  payload[16]);
    } else if (control == 2 || control == 3) {
        if (len != 7) {
            co_printf("    invalid len for control=2/3 (expect 7)\\r\\n");
            BleFunc_DumpPayload(payload, len);
            BleFunc_SendResultToRx(reply_cmd, 0x01);
            return;
        }
    } else {
        co_printf("    invalid delete_control\\r\\n");
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }

    BleFunc_DumpPayload(payload, len);

#if (!ENABLE_NFC_ADD_SIMULATION)
    /* [PRODUCTION Mode] 真实转发�?MCU */
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    /* [DEBUG-ONLY] 模拟 MCU 回复成功 */
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] Delete NFC Success (0x00)\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 5.13 手机查询 NFC 卡（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0EFE
 * - Payload: Time(6)
 * - Response Cmd: 0x0E01
 * - Response Payload: ResultCode(1) + (uuid/key 列表，可选，最�?6 �?
 * @note 未接入卡池时默认�?0x01（暂无卡）�?
 */
void BleFunc_FE_SearchNfc(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Search NFC (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 6) {
        co_printf("    invalid payload len=%d (expect 6)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x02);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    BleFunc_DumpPayload(payload, len);

/* NFC 卡查询建议交�?MCU 统一处理（卡�?权限/加密等在 MCU 侧更集中�?*/
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    /* [DEBUG-ONLY] 保留当前“默认无卡”的行为，同时镜像下�?MCU */
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    BleFunc_SendResultToRx(reply_cmd, 0x01);
#endif
}

/**
 * @brief 5.14 手机设置油车设防/解防（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0FFE
 * - Payload: Time(6) + DefendStatus(1)
 *   - 0x00 油车解防�?x01 油车设防
 * - Response Cmd: 0x0F01
 */
void BleFunc_FE_OilDefence(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Oil Defence (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    DefendStatus=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x0F01 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    {
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload,
                                 (uint16_t)len);
        co_printf("[DEBUG_SIM] Oil Defence Success (0x00)\\r\\n");
        BleFunc_SendResultToRx(reply_cmd, 0x00);
    }
#endif
}

/**
 * @brief 5.15 手机设置油车寻车声音（APP -> 设备�?
 * @details
 * - Request Cmd: 0x11FE
 * - Payload: Time(6) + sound(1)
 *   - 0x01 低档�?x02 中档�?x03 高档�?x04 关闭
 * - Response Cmd: 0x1101
 */
void BleFunc_FE_OilCarSearch(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Oil Car Search (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    sound=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1101 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    {
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload,
                                 (uint16_t)len);
        co_printf("[DEBUG_SIM] Oil Car Search Success (0x00)\\r\\n");
        BleFunc_SendResultToRx(reply_cmd, 0x00);
    }
#endif
}

/**
 * @brief 5.16 手机设置油车尾箱锁（APP -> 设备�?
 * @details
 * - Request Cmd: 0x12FE
 * - Payload: Time(6) + control(1)
 *   - 0x00 开锁；0x01 关锁
 * - Response Cmd: 0x1201
 */
void BleFunc_FE_SetBootLock(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Set Boot Lock (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1201 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    {
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload,
                                 (uint16_t)len);
        co_printf("[DEBUG_SIM] Set Boot Lock Success (0x00)\\r\\n");
        BleFunc_SendResultToRx(reply_cmd, 0x00);
    }
#endif
}

/**
 * @brief 5.16 手机设置 NFC 开关（APP -> 设备�?
 * @details
 * - Request Cmd: 0x13FE
 * - Payload: Time(6) + control(1)�?x00 关；0x01 开�?
 * - Response Cmd: 0x1301
 */
void BleFunc_FE_SetNfc(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Set NFC (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1301 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    {
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload,
                                 (uint16_t)len);
        co_printf("[DEBUG_SIM] Set NFC Success (0x00)\\r\\n");
        BleFunc_SendResultToRx(reply_cmd, 0x00);
    }
#endif
}

/**
 * @brief 5.17 手机设置座桶锁开关（鞍座锁）（APP -> 设备�?
 * @details
 * - Request Cmd: 0x14FE
 * - Payload: Time(6) + control(1)�?x00 开�?x01 关）
 * - Response Cmd: 0x1401
 */
void BleFunc_FE_SetSeatLock(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Set Seat Lock (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1401 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    {
        /* 调试模式：根�?control 字段发送不�?MCU 命令 */
        uint8_t ctrl     = payload[6]; /* 0x00 开/0x01 �?*/
        uint16_t mcu_cmd = (ctrl == 0x00u) ? (uint16_t)CMD_VEHICLE_UNLOCK_SEAT
                                           : (uint16_t)CMD_VEHICLE_LOCK_SEAT;
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, mcu_cmd, &ctrl, 1u);
        co_printf(
            "[DEBUG_SIM] Set Seat Lock mcu_cmd=0x%04X Success (0x00)\\r\\n",
            (unsigned)mcu_cmd);
        BleFunc_SendResultToRx(reply_cmd, 0x00);
    }
#endif
}

/**
 * @brief 5.18 手机设置静音开关（APP -> 设备�?
 * @details
 * - Request Cmd: 0x15FE
 * - Payload: Time(6) + control(1)
 *   - 0x00 开（声防盗设防）；0x01 关（静音设防�?
 * - Response Cmd: 0x1501
 * - Response ResultCode: 0x00 成功�?x01 失败�?x02 acc 状态拒绝（协议文档扩展�?
 */
void BleFunc_FE_SetCarMute(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Set Car Mute (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1501 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    {
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload,
                                 (uint16_t)len);
        co_printf("[DEBUG_SIM] Set Car Mute Success (0x00)\\r\\n");
        BleFunc_SendResultToRx(reply_cmd, 0x00);
    }
#endif
}

/**
 * @brief 5.19 手机设置中箱锁开关（APP -> 设备�?
 * @details
 * - Request Cmd: 0x16FE
 * - Payload: Time(6) + control(1)�?x00 开�?x01 关）
 * - Response Cmd: 0x1601
 */
void BleFunc_FE_SetMidBoxLock(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Set Mid Box Lock (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1601 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    {
        /* 调试模式：根�?control 字段发送不�?MCU 命令 */
        uint8_t ctrl     = payload[6]; /* 0x00 开/0x01 �?*/
        uint16_t mcu_cmd = (ctrl == 0x00u)
                               ? (uint16_t)CMD_VEHICLE_UNLOCK_MIDDLE_BOX
                               : (uint16_t)CMD_VEHICLE_LOCK_MIDDLE_BOX;
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, mcu_cmd, &ctrl, 1u);
        co_printf(
            "[DEBUG_SIM] Set Mid Box Lock mcu_cmd=0x%04X Success (0x00)\\r\\n",
            (unsigned)mcu_cmd);
        BleFunc_SendResultToRx(reply_cmd, 0x00);
    }
#endif
}

/**
 * @brief 5.20 手机设置紧急模式开关（APP -> 设备�?
 * @details
 * - Request Cmd: 0x17FE
 * - Payload: Time(6) + control(1)�?x01 打开�?x00 关闭�?
 * - Response Cmd: 0x1701
 */
void BleFunc_FE_SetEmergencyMode(uint16_t cmd, const uint8_t *payload,
                                 uint8_t len)
{
    co_printf("  -> Set Emergency Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1701 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    {
        /* 调试模式：根�?control 字段发送对�?MCU 命令 */
        uint8_t ctrl = payload[6]; /* 0x00 关闭/0x01 打开 */
        uint16_t mcu_cmd =
            (ctrl == 0x00u) ? 0x41u : 0x40u; /* 0x40=打开, 0x41=关闭 */
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, mcu_cmd, &ctrl, 1u);
        co_printf("[DEBUG_SIM] Set Emergency Mode Success (0x00)\\r\\n");
        BleFunc_SendResultToRx(reply_cmd, 0x00);
    }
#endif
}

/**
 * @brief 5.21 获取当前正在连接的手�?MAC（APP -> 设备�?
 * @details
 * - Request Cmd: 0x18FE
 * - Payload: Time(6)
 * - Response Cmd: 0x1801
 * - Response Payload: ResultCode(1) + MAC(6)
 *   - 成功：ResultCode=0x00 + 6字节 MAC
 *   - 失败：ResultCode=0x01 + FF FF FF FF FF FF
 *
 * @note 你给的文档中 5.22 “单控开锁”也标了 Cmd=0x18FE，但�?payload 多了
 * controlType�? 为兼容两种可能：
 *       - len==6 按“获取手�?MAC”处�?
 *       - len==7 按“单控开锁”处理（controlType=payload[6]），回包仍按
 * 0x1801+ResultCode(1)
 */
void BleFunc_FE_GetPhoneMac(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> 0x18FE Handler (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || (len != 6 && len != 7)) {
        co_printf("    invalid payload len=%d (expect 6 or 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        /* 失败回包：ResultCode(1)+MAC(6)=7 */
        uint8_t resp[7] = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        BleFunc_SendResultToRx_WithData(reply_cmd, resp,
                                        (uint16_t)sizeof(resp));
        return;
    }

    BleFunc_PrintTime6(&payload[0]);
    if (len == 7) {
        co_printf("    controlType=0x%02X\\r\\n", payload[6]);
        BleFunc_DumpPayload(payload, len);
/* len==7: 按“单控开锁”处理，下发�?MCU 并等待回�?超时再回 0x1801 */
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx = Protocol_Get_Rx_Conidx();
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)cmd, payload, (uint16_t)len);
        }
#else
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload,
                                 (uint16_t)len);
        co_printf("[DEBUG_SIM] Single Control Unlock Success (0x00)\\r\\n");
        BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
        return;
    }

    /* len == 6: 获取手机 MAC */
    BleFunc_DumpPayload(payload, len);

    uint8_t resp[7] = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t conidx  = Protocol_Get_Rx_Conidx();
    uint8_t mac6[6] = {0};
    if (RSSI_Check_Get_Peer_Addr(conidx, mac6)) {
        resp[0] = 0x00;
        memcpy(&resp[1], mac6, 6);
    }
    BleFunc_SendResultToRx_WithData(reply_cmd, resp, (uint16_t)sizeof(resp));
}

/**
 * @brief 5.22 单控开锁（APP -> 设备�?
 * @details
 * - Request Cmd: 0x19FE（以工程命令�?protocol_cmd.h 为准�?
 * - Payload: Time(6) + controlType(1)
 * - Response Cmd: 0x1901
 * - Response Payload: ResultCode(1)
 * @note
 * - 你提供的文档里该功能曾写�?0x18FE，但本工程已�?0x18FE 用作“获取手�?MAC”�?
 * - 为避免命令号冲突导致路由/回包错误，这里按 0x19FE 实现�?
 */
void BleFunc_FE_SetUnlockMode(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Set Unlock Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd); /* 0x1901 */
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /* 5.22 单控开锁：Time(6) + controlType(1) */
    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }

    BleFunc_PrintTime6(&payload[0]);
    uint8_t controlType = payload[6];
    co_printf("    controlType=0x%02X\\r\\n", controlType);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1901 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] Set Unlock Mode Success (0x00)\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 5.23 充电显示器开关（APP -> 设备�?
 * @details
 * - Request Cmd: 0x1AFE
 * - Payload: Time(6) + control(1)
 *   - control: 0x01 开启充电显示器�?x00 关闭充电显示�?
 * - Response Cmd: 0x1A01
 * - Response Payload: ResultCode(1)
 * - MCU 命令: 0x200 开�?/ 0x201 关闭
 */
void BleFunc_FE_ChargeDisplay(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> Charge Display (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd); /* 0x1A01 */
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /* 0x1AFE: Time(6) + control(1) = 7 bytes */
    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (expect >= 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }

    BleFunc_PrintTime6(&payload[0]);
    uint8_t control = payload[6];
    co_printf("    control=0x%02X (%s)\\r\\n", control,
              (control == 0x01u) ? "ON" : "OFF");
    BleFunc_DumpPayload(payload, len);

/* 下发�?MCU：control=0x01 -> CMD_CHARGE_DISPLAY_ON(0x200)；control=0x00 ->
 * CMD_CHARGE_DISPLAY_OFF(0x201) */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    {
        /* 调试模式：根�?control 字段发送不�?MCU 命令 */
        uint16_t mcu_cmd = (control == 0x01u)
                               ? (uint16_t)CMD_CHARGE_DISPLAY_ON
                               : (uint16_t)CMD_CHARGE_DISPLAY_OFF;
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, mcu_cmd, &control, 1u);
        co_printf(
            "[DEBUG_SIM] Charge Display mcu_cmd=0x%04X Success (0x00)\\r\\n",
            (unsigned)mcu_cmd);
        BleFunc_SendResultToRx(reply_cmd, 0x00);
    }
#endif
}

/**
 * @brief 6.2 助力推车参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x02FD
 * - Payload: Time(6) + control(1) + speed(1)
 * - Response Cmd: 0x0202
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_AssistiveTrolley(uint16_t cmd, const uint8_t *payload,
                                 uint8_t len)
{
    co_printf("  -> FD Assistive Trolley (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 8) {
        co_printf("    invalid payload len=%d (expect 8)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X speed=%u\\r\\n", payload[6],
              (unsigned)payload[7]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x0202 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.3 延时大灯参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x03FD
 * - Payload: Time(6) + control(1) + delayTime(1)
 * - Response Cmd: 0x0302
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_DelayedHeadlight(uint16_t cmd, const uint8_t *payload,
                                 uint8_t len)
{
    co_printf("  -> FD Delayed Headlight (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 8) {
        co_printf("    invalid payload len=%d (expect 8)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X delayTime=%us\\r\\n", payload[6],
              (unsigned)payload[7]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x0302 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.4 充电功率参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x04FD
 * - Payload: Time(6) + chargingPower(1)
 * - Response Cmd: 0x0402
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetChargingPower(uint16_t cmd, const uint8_t *payload,
                                 uint8_t len)
{
    co_printf("  -> FD Set Charging Power (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    chargingPower=%u\\r\\n", (unsigned)payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x0402 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.5 自动进入 P 档参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x05FD
 * - Payload: Time(6) + control(1) + delayTime(1)
 * - Response Cmd: 0x0502
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetPGearMode(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set P Gear Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 8) {
        co_printf("    invalid payload len=%d (expect 8)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X delayTime=%us\\r\\n", payload[6],
              (unsigned)payload[7]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x0502 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.6 和弦喇叭参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x06FD
 * - Payload: Time(6) + soundSource(1) + volume(1)
 * - Response Cmd: 0x0602
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetChordHornMode(uint16_t cmd, const uint8_t *payload,
                                 uint8_t len)
{
    co_printf("  -> FD Set Chord Horn Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 8) {
        co_printf("    invalid payload len=%d (expect 8)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    soundSource=%u volume=%u\\r\\n", (unsigned)payload[6],
              (unsigned)payload[7]);
    BleFunc_DumpPayload(payload, len);
/*
 * 下发�?MCU，并等待回包/超时再回 0x0602
 *
 * @why
 * - BLE(0x06FD) payload 是复合字段：Time6 + soundSource(1) + volume(1)
 * - MCU 协议要求分两条命令：0x59(音源1B) + 0x60(音量1B)
 * - 若整�?8B 透传�?MCU，会导致 MCU 侧按 1B 解析失败/不回包，最�?BLE 事务超时
 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx       = Protocol_Get_Rx_Conidx();
        uint8_t sound_source = payload[6];
        uint8_t volume       = payload[7];

        /* 启动链式事务：step0=等待音源设置回包；收到成功后自动发音量设�?*/
        s_mcu_pending.chain_kind = BLEFUNC_MCU_CHAIN_CHORD_HORN_06FD;
        s_mcu_pending.chain_step = 0u;
        s_mcu_pending.chain_u8_0 = volume;

        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)CMD_Chord_horn_type_set, &sound_source,
                             1u);
    }
#else
    {
        /* 模拟回包也按 MCU 协议拆分下发，方便你抓串口确认数据内�?*/
        uint8_t sound_source = payload[6];
        uint8_t volume       = payload[7];
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Chord_horn_type_set,
                                 &sound_source, 1u);
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Chord_horn_volume_set, &volume,
                                 1u);
    }
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.7 三色氛围灯参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x07FD
 * - Payload: Time(6) + lightingEffects(1) + gradual(1) + rgbr(1) + rgbg(1) +
 * rgbb(1)
 *   - 字段合计长度 = 6 + 1 + 1 + 1 + 1 + 1 = 11
 * - Response Cmd: 0x0702
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetRgbLightMode(uint16_t cmd, const uint8_t *payload,
                                uint8_t len)
{
    co_printf("  -> FD Set RGB Light Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /*
     * 兼容�?
     * - 文档字段数对�?11 字节；当�?App 实测也发 11 字节（AES 解密�?unpad 得到
     * 11）�?
     * - 若后续扩展为 12 字节，这里允�?len==12，只把第 12
     * 字节当作扩展字段打印，不影响 MCU 透传�?
     */
    if (payload == NULL || len < 11) {
        co_printf("    invalid payload len=%d (expect >= 11)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    effects=%u gradual=%u rgb=(%u,%u,%u)\\r\\n",
              (unsigned)payload[6], (unsigned)payload[7], (unsigned)payload[8],
              (unsigned)payload[9], (unsigned)payload[10]);
    if (len >= 12) { co_printf("    ext=0x%02X\\r\\n", (unsigned)payload[11]); }
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x0702 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.8 辅助倒车档开关参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x08FD
 * - Payload: Time(6) + control(1) + reverseSpeed(1)
 * - Response Cmd: 0x0802
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetAuxiliaryParking(uint16_t cmd, const uint8_t *payload,
                                    uint8_t len)
{
    co_printf("  -> FD Set Auxiliary Parking (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 8) {
        co_printf("    invalid payload len=%d (expect 8)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X reverseSpeed=%u\\r\\n", payload[6],
              (unsigned)payload[7]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x0802 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.9 智能开关设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x63FD
 * - Payload: Time(6) + control(1) + controlType(1)
 * - Response Cmd: 0x6302
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetIntelligentSwitch(uint16_t cmd, const uint8_t *payload,
                                     uint8_t len)
{
    co_printf("  -> FD Set Intelligent Switch (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (expect >= 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    uint16_t payload_len_before_strip = len;
    uint16_t temp_len                 = len;
    BleFunc_StripTime6_ForMcu(&payload, &temp_len);
    len                 = (uint8_t)temp_len;
    uint8_t control     = payload[0];
    uint8_t controlType = (len >= 2) ? payload[1] : 0;
    /*
     * controlType mapping (per on-site confirmation):
     * - 0x02: saddle switch
     * - 0x03: ABS switch
     * - 0x04: power LED
     * - 0x05: position light
     * - 0x0A: three-color ambient
     */
    const char *controlTypeStr = "unknown";
    switch (controlType) {
        case 0x02u:
            controlTypeStr = "saddle_switch";
            break;
        case 0x03u:
            controlTypeStr = "abs_switch";
            break;
        case 0x04u:
            controlTypeStr = "power_led";
            break;
        case 0x05u:
            controlTypeStr = "position_light";
            break;
        case 0x0Au:
            controlTypeStr = "three_color_ambient";
            break;
        default:
            break;
    }
    co_printf("    strip %u->%u control=0x%02X controlType=0x%02X(%s)\\r\\n",
              (unsigned)payload_len_before_strip, (unsigned)len, control,
              controlType, controlTypeStr);
    BleFunc_DumpPayload(payload, len);

/*
 * 0x63FD strategy:
 * - 0x0A: use dedicated MCU cmd, no data
 * - 0x02/0x03/0x04/0x05: send 1B control to MCU cmd
 * - others: passthrough 0x63FD (control+type)
 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        if (controlType == 0x0Au) {
            uint16_t mcu_id       = 0u;
            const char *actionStr = "invalid";
            if (control == 0x01u)
                mcu_id = (uint16_t)CMD_Three_shooting_lamp_on;
            else if (control == 0x00u)
                mcu_id = (uint16_t)CMD_Three_shooting_lamp_off;
            else if (control == 0x02u)
                mcu_id = (uint16_t)CMD_Three_shooting_lamp_default;
            else
                mcu_id = 0u;
            if (mcu_id == (uint16_t)CMD_Three_shooting_lamp_on)
                actionStr = "ON(0x61)";
            else if (mcu_id == (uint16_t)CMD_Three_shooting_lamp_off)
                actionStr = "OFF(0x62)";
            else if (mcu_id == (uint16_t)CMD_Three_shooting_lamp_default)
                actionStr = "DEFAULT(0x63)";

            co_printf("    three_shooting_lamp action=%s mcu_id=0x%04X\\r\\n",
                      actionStr, (unsigned)mcu_id);
            if (mcu_id == 0u) {
                BleFunc_SendResultToRx(reply_cmd, 0x01);
                return;
            }
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, mcu_id,
                                 NULL, 0u);
        } else if (controlType == 0x02u || controlType == 0x03u ||
                   controlType == 0x04u || controlType == 0x05u) {
            if (control != 0x00u && control != 0x01u) {
                BleFunc_SendResultToRx(reply_cmd, 0x01);
                return;
            }
            uint16_t mcu_id = 0u;
            switch (controlType) {
                case 0x02u:
                    mcu_id = (uint16_t)CMD_SADDLE_ON_OFF;
                    break;
                case 0x03u:
                    mcu_id = (uint16_t)CMD_ABS_ON_OFF;
                    break;
                case 0x04u:
                    mcu_id = (uint16_t)CMD_POWER_LED;
                    break;
                case 0x05u:
                    mcu_id = (uint16_t)CMD_Position_light_on_off;
                    break;
                default:
                    break;
            }
            uint8_t ctrl = control;
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, mcu_id,
                                 &ctrl, 1u);
        } else {
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)cmd, payload, (uint16_t)len);
        }
    }
#else
    if (controlType == 0x0Au) {
        uint16_t mcu_id       = 0u;
        const char *actionStr = "invalid";
        if (control == 0x01u)
            mcu_id = (uint16_t)CMD_Three_shooting_lamp_on;
        else if (control == 0x00u)
            mcu_id = (uint16_t)CMD_Three_shooting_lamp_off;
        else if (control == 0x02u)
            mcu_id = (uint16_t)CMD_Three_shooting_lamp_default;
        else
            mcu_id = 0u;
        if (mcu_id == (uint16_t)CMD_Three_shooting_lamp_on)
            actionStr = "ON(0x61)";
        else if (mcu_id == (uint16_t)CMD_Three_shooting_lamp_off)
            actionStr = "OFF(0x62)";
        // else if (mcu_id == (uint16_t)CMD_Three_shooting_lamp_default)
        // actionStr = "DEFAULT(0x63)";

        co_printf(
            "[DEBUG_SIM] three_shooting_lamp action=%s mcu_id=0x%04X\\r\\n",
            actionStr, (unsigned)mcu_id);
        if (mcu_id != 0u) {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, mcu_id, NULL, 0u);
        }
    } else if (controlType == 0x02u || controlType == 0x03u ||
               controlType == 0x04u || controlType == 0x05u) {
        if (control != 0x00u && control != 0x01u) {
            BleFunc_SendResultToRx(reply_cmd, 0x01);
            return;
        }
        uint16_t mcu_id = 0u;
        switch (controlType) {
            case 0x02u:
                mcu_id = (uint16_t)CMD_SADDLE_ON_OFF;
                break;
            case 0x03u:
                mcu_id = (uint16_t)CMD_ABS_ON_OFF;
                break;
            case 0x04u:
                mcu_id = (uint16_t)CMD_POWER_LED;
                break;
            case 0x05u:
                mcu_id = (uint16_t)CMD_Position_light_on_off;
                break;
            default:
                break;
        }
        if (mcu_id != 0u) {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, mcu_id, &control, 1u);
        }
    } else {
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, (uint16_t)cmd, payload,
                                 (uint16_t)len);
    }
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.10/6.23 参数同步类（兜底处理�?
 * @details
 * - 该类命令在文档中多为“设�?-> APP”主动上报�?
 * - 工程命令表保留了�?Cmd 的解析入口，这里做最小解析：至少包含 Time(6)�?
 * - Response Cmd: 0x6402
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_ParamSynchronize(uint16_t cmd, const uint8_t *payload,
                                 uint8_t len)
{
    co_printf("  -> FD Param Synchronize (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len < 6) {
        co_printf("    invalid payload len=%d (min 6)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    BleFunc_DumpPayload(payload, len);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
}

/**
 * @brief 6.11 参数同步（有变化时，兜底处理�?
 * @details
 * - Request Cmd: 0x66FD
 * - Payload: 至少包含 Time(6)
 * - Response Cmd: 0x6602
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_ParamSynchronizeChange(uint16_t cmd, const uint8_t *payload,
                                       uint8_t len)
{
    co_printf("  -> FD Param Sync Change (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len < 6) {
        co_printf("    invalid payload len=%d (min 6)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    BleFunc_DumpPayload(payload, len);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
}

/**
 * @brief 6.12 恢复默认（APP -> 设备�?
 * @details
 * - Request Cmd: 0x65FD
 * - Payload: Time(6) + code(1) + codeSub(1)
 * - Response Cmd: 0x6502
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetDefaultMode(uint16_t cmd, const uint8_t *payload,
                               uint8_t len)
{
    co_printf("  -> FD Set Default Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 8) {
        co_printf("    invalid payload len=%d (expect 8)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    code=0x%02X codeSub=0x%02X\\r\\n", payload[6], payload[7]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x6502 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.13 车辆防盗告警设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x09FD
 * - Payload: Time(6) + alarmSensitivity(1) + alarmVolume(1) + soundDefence(1)
 * - Response Cmd: 0x0902
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetVichleGurdMode(uint16_t cmd, const uint8_t *payload,
                                  uint8_t len)
{
    co_printf("  -> FD Set Vehicle Guard Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 9) {
        co_printf("    invalid payload len=%d (expect 9)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    sensitivity=0x%02X volume=0x%02X soundDefence=0x%02X\\r\\n",
              payload[6], payload[7], payload[8]);
    BleFunc_DumpPayload(payload, len);

/* Send sensitivity + volume + soundDefence to MCU (Time6 stripped in transport)
 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.14 自动转向回位设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0AFD
 * - Payload: Time(6) + control(1)
 * - Response Cmd: 0x0A02
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetAutoReturnMode(uint16_t cmd, const uint8_t *payload,
                                  uint8_t len)
{
    co_printf("  -> FD Set Auto Return Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x0A02 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.15 EBS 开关设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0BFD
 * - Payload: Time(6) + control(1)
 * - Response Cmd: 0x0B02
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetEbsSwitch(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set EBS Switch (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x0B02 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.16 低速挡(E-SAVE)参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0CFD
 * - Payload: control(1) + speed(1)（无 Time6�?
 *   - control: 0x01 开启并设置档位�?x00 关闭�?x02 默认
 *   - speed: 档位值（1B，约�?0/1/2/3 对应 00/01/10/11�?
 * - Response Cmd: 0x0C02
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetESaveMode(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set E-SAVE Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len < 1) {
        co_printf("    invalid payload len=%d (expect >= 1)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    const uint8_t *mcu_payload        = payload;
    uint16_t payload_len_before_strip = len;
    uint16_t temp_len                 = len;
    if (len >= 7) {
        BleFunc_PrintTime6(&payload[0]);
        BleFunc_StripTime6_ForMcu(&mcu_payload, &temp_len);
    }
    len = (uint8_t)temp_len;
    if (len < 1u) {
        co_printf("    invalid payload len after strip=%u (expect >= 1)\\r\\n",
                  (unsigned)len);
        BleFunc_DumpPayload(mcu_payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    uint8_t control = mcu_payload[0];
    uint8_t speed   = (len >= 2) ? mcu_payload[1] : control;
    co_printf("    strip %u->%u control=0x%02X speed=%u\\r\\n",
              (unsigned)payload_len_before_strip, (unsigned)len, control,
              (unsigned)speed);
    BleFunc_DumpPayload(mcu_payload, len);

    /* New spec: 1B control (0..3) as torque level */
    if (len == 1u) {
        if (control > 0x03u) {
            co_printf("    invalid control(level)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x01);
            return;
        }
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx            = Protocol_Get_Rx_Conidx();
            s_mcu_pending.chain_kind  = BLEFUNC_MCU_CHAIN_GEAR_SET_FD;
            s_mcu_pending.chain_step  = 0u;
            s_mcu_pending.chain_u8_0  = speed;
            s_mcu_pending.chain_u16_0 = (uint16_t)CMD_Low_speed_gear_speed_set;
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Low_speed_gear_on, NULL, 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Low_speed_gear_on, NULL, 0u);
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Low_speed_gear_speed_set,
                                     &speed, 1u);
            co_printf("[DEBUG_SIM] FD E-SAVE split send Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }

    /* 规则：收到该指令后“拆分下发”给 MCU：先 on，再 speed_set（并等待�?2
     * 条回包后再回�?BLE�?*/
    if (control == 0x01u) {
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx            = Protocol_Get_Rx_Conidx();
            s_mcu_pending.chain_kind  = BLEFUNC_MCU_CHAIN_GEAR_SET_FD;
            s_mcu_pending.chain_step  = 0u;
            s_mcu_pending.chain_u8_0  = speed;
            s_mcu_pending.chain_u16_0 = (uint16_t)CMD_Low_speed_gear_speed_set;
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Low_speed_gear_on, NULL, 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Low_speed_gear_on, NULL, 0u);
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Low_speed_gear_speed_set,
                                     &speed, 1u);
            co_printf("[DEBUG_SIM] FD E-SAVE split send Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }
    if (control == 0x00u) {
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx = Protocol_Get_Rx_Conidx();
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Low_speed_gear_off, NULL, 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Low_speed_gear_off, NULL,
                                     0u);
            co_printf("[DEBUG_SIM] FD E-SAVE off Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }
    if (control == 0x02u) {
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx = Protocol_Get_Rx_Conidx();
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Low_speed_gear_default, NULL,
                                 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Low_speed_gear_default, NULL,
                                     0u);
            co_printf("[DEBUG_SIM] FD E-SAVE default Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }

    co_printf("    invalid control\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x01);
}

/**
 * @brief 6.17 中速挡(DYN)参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0DFD
 * - Payload: control(1) + speed(1)（无 Time6�?
 * - Response Cmd: 0x0D02
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetDynMode(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set DYN Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len < 1) {
        co_printf("    invalid payload len=%d (expect >= 1)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    const uint8_t *mcu_payload        = payload;
    uint16_t payload_len_before_strip = len;
    uint16_t temp_len                 = len;
    if (len >= 7) {
        BleFunc_PrintTime6(&payload[0]);
        BleFunc_StripTime6_ForMcu(&mcu_payload, &temp_len);
    }
    len = (uint8_t)temp_len;
    if (len < 1u) {
        co_printf("    invalid payload len after strip=%u (expect >= 1)\\r\\n",
                  (unsigned)len);
        BleFunc_DumpPayload(mcu_payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    uint8_t control = mcu_payload[0];
    uint8_t speed   = (len >= 2) ? mcu_payload[1] : control;
    co_printf("    strip %u->%u control=0x%02X speed=%u\\r\\n",
              (unsigned)payload_len_before_strip, (unsigned)len, control,
              (unsigned)speed);
    BleFunc_DumpPayload(mcu_payload, len);

    /* New spec: 1B control (0..3) as torque level */
    if (len == 1u) {
        if (control > 0x03u) {
            co_printf("    invalid control(level)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x01);
            return;
        }
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx           = Protocol_Get_Rx_Conidx();
            s_mcu_pending.chain_kind = BLEFUNC_MCU_CHAIN_GEAR_SET_FD;
            s_mcu_pending.chain_step = 0u;
            s_mcu_pending.chain_u8_0 = speed;
            s_mcu_pending.chain_u16_0 =
                (uint16_t)CMD_Medium_speed_gear_speed_set;
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Medium_speed_gear_on, NULL, 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Medium_speed_gear_on, NULL,
                                     0u);
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Medium_speed_gear_speed_set,
                                     &speed, 1u);
            co_printf("[DEBUG_SIM] FD DYN split send Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }

    if (control == 0x01u) {
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx           = Protocol_Get_Rx_Conidx();
            s_mcu_pending.chain_kind = BLEFUNC_MCU_CHAIN_GEAR_SET_FD;
            s_mcu_pending.chain_step = 0u;
            s_mcu_pending.chain_u8_0 = speed;
            s_mcu_pending.chain_u16_0 =
                (uint16_t)CMD_Medium_speed_gear_speed_set;
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Medium_speed_gear_on, NULL, 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Medium_speed_gear_on, NULL,
                                     0u);
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Medium_speed_gear_speed_set,
                                     &speed, 1u);
            co_printf("[DEBUG_SIM] FD DYN split send Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }
    if (control == 0x00u) {
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx = Protocol_Get_Rx_Conidx();
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Medium_speed_gear_off, NULL, 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Medium_speed_gear_off, NULL,
                                     0u);
            co_printf("[DEBUG_SIM] FD DYN off Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }
    if (control == 0x02u) {
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx = Protocol_Get_Rx_Conidx();
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Medium_speed_gear_default, NULL,
                                 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_Medium_speed_gear_default,
                                     NULL, 0u);
            co_printf("[DEBUG_SIM] FD DYN default Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }

    co_printf("    invalid control\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x01);
}

/**
 * @brief 6.18 高速档(SPORT)参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0EFD
 * - Payload: control(1) + speed(1)（无 Time6�?
 * - Response Cmd: 0x0E02
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetSportMode(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set SPORT Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len < 1) {
        co_printf("    invalid payload len=%d (expect >= 1)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    const uint8_t *mcu_payload        = payload;
    uint16_t payload_len_before_strip = len;
    uint16_t temp_len                 = len;
    if (len >= 7) {
        BleFunc_PrintTime6(&payload[0]);
        BleFunc_StripTime6_ForMcu(&mcu_payload, &temp_len);
    }
    len = (uint8_t)temp_len;
    if (len < 1u) {
        co_printf("    invalid payload len after strip=%u (expect >= 1)\\r\\n",
                  (unsigned)len);
        BleFunc_DumpPayload(mcu_payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    uint8_t control = mcu_payload[0];
    uint8_t speed   = (len >= 2) ? mcu_payload[1] : control;
    co_printf("    strip %u->%u control=0x%02X speed=%u\\r\\n",
              (unsigned)payload_len_before_strip, (unsigned)len, control,
              (unsigned)speed);
    BleFunc_DumpPayload(mcu_payload, len);

    /* New spec: 1B control (0..3) as torque level */
    if (len == 1u) {
        if (control > 0x03u) {
            co_printf("    invalid control(level)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x01);
            return;
        }
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx            = Protocol_Get_Rx_Conidx();
            s_mcu_pending.chain_kind  = BLEFUNC_MCU_CHAIN_GEAR_SET_FD;
            s_mcu_pending.chain_step  = 0u;
            s_mcu_pending.chain_u8_0  = speed;
            s_mcu_pending.chain_u16_0 = (uint16_t)CMD_High_speed_gear_speed_set;
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_High_speed_gear_on, NULL, 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_High_speed_gear_on, NULL,
                                     0u);
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_High_speed_gear_speed_set,
                                     &speed, 1u);
            co_printf("[DEBUG_SIM] FD SPORT split send Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }

    if (control == 0x01u) {
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx            = Protocol_Get_Rx_Conidx();
            s_mcu_pending.chain_kind  = BLEFUNC_MCU_CHAIN_GEAR_SET_FD;
            s_mcu_pending.chain_step  = 0u;
            s_mcu_pending.chain_u8_0  = speed;
            s_mcu_pending.chain_u16_0 = (uint16_t)CMD_High_speed_gear_speed_set;
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_High_speed_gear_on, NULL, 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_High_speed_gear_on, NULL,
                                     0u);
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_High_speed_gear_speed_set,
                                     &speed, 1u);
            co_printf("[DEBUG_SIM] FD SPORT split send Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }
    if (control == 0x00u) {
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx = Protocol_Get_Rx_Conidx();
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_High_speed_gear_off, NULL, 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_High_speed_gear_off, NULL,
                                     0u);
            co_printf("[DEBUG_SIM] FD SPORT off Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }
    if (control == 0x02u) {
#if (!ENABLE_NFC_ADD_SIMULATION)
        {
            uint8_t conidx = Protocol_Get_Rx_Conidx();
            BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_High_speed_gear_default, NULL,
                                 0u);
        }
#else
        {
            BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                     (uint16_t)CMD_High_speed_gear_default,
                                     NULL, 0u);
            co_printf("[DEBUG_SIM] FD SPORT default Success (0x00)\\r\\n");
            BleFunc_SendResultToRx(reply_cmd, 0x00);
        }
#endif
        return;
    }

    co_printf("    invalid control\\r\\n");
    BleFunc_SendResultToRx(reply_cmd, 0x01);
}

/**
 * @brief 6.19 丢失模式参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x0FFD
 * - Payload: Time(6) + control(1)
 * - Response Cmd: 0x0F02
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetLostMode(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set Lost Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x0F02 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.20 TCS 开关参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x10FD
 * - Payload: Time(6) + control(1)
 * - Response Cmd: 0x1002
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetTcsSwitch(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set TCS Switch (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1002 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.21 侧支架开关参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x11FD
 * - Payload: Time(6) + control(1)
 * - Response Cmd: 0x1102
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetSideStand(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set Side Stand (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 7) {
        co_printf("    invalid payload len=%d (expect 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    control=0x%02X\\r\\n", payload[6]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1102 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.22 电池类型与容量参数设置（APP -> 设备�?
 * @details
 * - Request Cmd: 0x12FD
 * - Payload: Time(6) + type(1) + capacity(1)
 * - Response Cmd: 0x1202
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FD_SetBatteryParameter(uint16_t cmd, const uint8_t *payload,
                                    uint8_t len)
{
    co_printf("  -> FD Set Battery Param (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len != 8) {
        co_printf("    invalid payload len=%d (expect 8)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    co_printf("    type=0x%02X capacity=0x%02X\\r\\n", payload[6], payload[7]);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1202 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.23 数据同步类（兜底处理�?
 * @details
 * - 2KFA/2KGA/2MQA 数据同步在文档中是“设�?-> APP”上报类�?
 * - 工程保留了解析入口，这里做兜底：解析 Time(6) 并回 ACK 风格 ResultCode�?
 * - Response Cmd: 0x1302
 */
void BleFunc_FD_SetUpdataApp(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Update APP / Data Sync (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    if (payload == NULL || len < 6) {
        co_printf("    invalid payload len=%d (min 6)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    BleFunc_DumpPayload(payload, len);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
}

/**
 * @brief 6.24 HDC 设置（兜底处理）
 * @details
 * - 文档为“设�?-> APP”上报类；工程保留解析入口�?
 * - Payload: 至少包含 Time(6)
 * - Response Cmd: 0x1402
 */
void BleFunc_FD_SetHdcMode(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set HDC Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;
    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (expect >= 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    uint8_t control = payload[6];
    if (control != 0x00u && control != 0x01u) {
        co_printf("    invalid control=0x%02X\\r\\n", control);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    co_printf("    control=0x%02X\\r\\n", control);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1402 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.25 HHC 设置（兜底处理）
 * @details
 * - 文档为“设备-> APP”上报类；工程保留解析入口�?
 * - Payload: 至少包含 Time(6)
 * - Response Cmd: 0x1502
 */
void BleFunc_FD_SetHhcMode(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set HHC Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;
    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (expect >= 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    uint8_t control = payload[6];
    if (control != 0x00u && control != 0x01u) {
        co_printf("    invalid control=0x%02X\\r\\n", control);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    co_printf("    control=0x%02X\\r\\n", control);
    BleFunc_DumpPayload(payload, len);
/* 下发�?MCU，并等待回包/超时再回 0x1502 */
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)cmd, payload, (uint16_t)len);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.26 起步强度状态设置（兜底处理�?
 * @details
 * - 文档为“设�?-> APP”上报类；工程保留解析入口�?
 * - Payload: 至少包含 Time(6)
 * - Response Cmd: 0x1602
 */
void BleFunc_FD_SetStartAbility(uint16_t cmd, const uint8_t *payload,
                                uint8_t len)
{
    co_printf("  -> FD Set Start Ability (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;

    /*
     * 按“通用蓝牙协议”常规格式实现（APP->SOC->MCU）：
     * - Payload: Time(6) + control(1) + strength(1)
     * - MCU: CMD_Starting_strength_set(0x0104)
     *
     * @why 这样做：
     * - 之前这里是兜底直接回 0x00，但 MCU
     * 没收到任何有效设置，导致“起步加速不生效”�?
     * - 你要求“开�?强度分开”，这里至少把两个字段解析出来并组合�?MCU data(2B)
     * 下发�?
     */
    if (payload == NULL || len != 8) {
        co_printf("    invalid payload len=%d (expect 8)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    uint8_t control  = payload[6];
    uint8_t strength = payload[7];
    co_printf("    control=0x%02X strength=0x%02X\\r\\n", control, strength);
    BleFunc_DumpPayload(payload, len);

    uint8_t mcu_data[2];
    mcu_data[0] = control;
    mcu_data[1] = strength;

#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)CMD_Starting_strength_set, mcu_data,
                             (uint16_t)sizeof(mcu_data));
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                             (uint16_t)CMD_Starting_strength_set, mcu_data,
                             (uint16_t)sizeof(mcu_data));
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.27 SPORT 档速度设置（兜底处理）
 * @details
 * - 文档为“设�?-> APP”上报类；工程保留解析入口�?
 * - Payload: 至少包含 Time(6)
 * - Response Cmd: 0x1702
 */
void BleFunc_FD_SetSportPowerSpeed(uint16_t cmd, const uint8_t *payload,
                                   uint8_t len)
{
    co_printf("  -> FD Set Sport Power Speed (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;
    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (expect >= 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    const uint8_t *mcu_payload = payload;
    uint16_t temp_len          = len;
    BleFunc_StripTime6_ForMcu(&mcu_payload, &temp_len);
    len = (uint8_t)temp_len;
    if (len < 1u) {
        co_printf("    invalid payload len after strip=%u (expect >= 1)\\r\\n",
                  (unsigned)len);
        BleFunc_DumpPayload(mcu_payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    uint8_t speed = mcu_payload[0];
    co_printf("    speed=%u\\r\\n", (unsigned)speed);
    BleFunc_DumpPayload(mcu_payload, len);
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)CMD_SPORT_mode_type_set, &speed, 1u);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                             (uint16_t)CMD_SPORT_mode_type_set, &speed, 1u);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.28 ECO 档速度设置（兜底处理）
 * @details
 * - 文档为“设�?-> APP”上报类；工程保留解析入口�?
 * - Payload: 至少包含 Time(6)
 * - Response Cmd: 0x1802
 */
void BleFunc_FD_SetEcoMode(uint16_t cmd, const uint8_t *payload, uint8_t len)
{
    co_printf("  -> FD Set ECO Mode (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;
    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (expect >= 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    const uint8_t *mcu_payload = payload;
    uint16_t temp_len          = len;
    BleFunc_StripTime6_ForMcu(&mcu_payload, &temp_len);
    len = (uint8_t)temp_len;
    if (len < 1u) {
        co_printf("    invalid payload len after strip=%u (expect >= 1)\\r\\n",
                  (unsigned)len);
        BleFunc_DumpPayload(mcu_payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    uint8_t speed = mcu_payload[0];
    co_printf("    speed=%u\\r\\n", (unsigned)speed);
    BleFunc_DumpPayload(mcu_payload, len);
#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE,
                             (uint16_t)CMD_ECO_mode_type_set, &speed, 1u);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                             (uint16_t)CMD_ECO_mode_type_set, &speed, 1u);
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}

/**
 * @brief 6.29 雷达设置（兜底处理）
 * @details
 * - 文档为“设�?-> APP”状态同�?上报类；工程保留解析入口�?
 * - Payload: 至少包含 Time(6)
 * - Response Cmd: 0x1902
 */
void BleFunc_FD_SetRadarSwitch(uint16_t cmd, const uint8_t *payload,
                               uint8_t len)
{
    co_printf("  -> FD Set Radar Switch (0x%04X)\\r\\n", cmd);
    uint16_t reply_cmd = BleFunc_MakeReplyCmd_FD(cmd);
    if (!BleFunc_EnsureAuthed(reply_cmd)) return;
    if (payload == NULL || len < 7) {
        co_printf("    invalid payload len=%d (expect >= 7)\\r\\n", (int)len);
        BleFunc_DumpPayload(payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    BleFunc_PrintTime6(&payload[0]);
    const uint8_t *mcu_payload = payload;
    uint16_t temp_len          = len;
    BleFunc_StripTime6_ForMcu(&mcu_payload, &temp_len);
    len = (uint8_t)temp_len;
    if (len < 1u) {
        co_printf("    invalid payload len after strip=%u (expect >= 1)\\r\\n",
                  (unsigned)len);
        BleFunc_DumpPayload(mcu_payload, len);
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    uint8_t control      = mcu_payload[0];
    bool has_sensitivity = (len >= 2u);
    uint8_t sensitivity  = has_sensitivity ? mcu_payload[1] : 0u;
    co_printf("    control=0x%02X sensitivity=0x%02X%s\\r\\n",
              (unsigned)control, (unsigned)sensitivity,
              has_sensitivity ? "" : " (n/a)");
    BleFunc_DumpPayload(mcu_payload, len);

    uint16_t switch_cmd = 0u;
    if (control == 0x00u)
        switch_cmd = (uint16_t)CMD_Radar_switch_on;
    else if (control == 0x01u)
        switch_cmd = (uint16_t)CMD_Radar_switch_off;
    else if (control == 0x02u)
        switch_cmd = (uint16_t)CMD_Radar_switch_default;
    if (switch_cmd == 0u) {
        co_printf("    invalid control\\r\\n");
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }
    if (has_sensitivity && sensitivity > 0x02u) {
        co_printf("    invalid sensitivity\\r\\n");
        BleFunc_SendResultToRx(reply_cmd, 0x01);
        return;
    }

#if (!ENABLE_NFC_ADD_SIMULATION)
    {
        uint8_t conidx = Protocol_Get_Rx_Conidx();
        if (has_sensitivity) {
            s_mcu_pending.chain_kind  = BLEFUNC_MCU_CHAIN_RADAR_SET_FD;
            s_mcu_pending.chain_step  = 0u;
            s_mcu_pending.chain_u8_0  = sensitivity;
            s_mcu_pending.chain_u16_0 = (uint16_t)CMD_Radar_sensitivity_set;
        }
        BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, switch_cmd,
                             NULL, 0u);
    }
#else
    BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, switch_cmd, NULL, 0u);
    if (has_sensitivity) {
        BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE,
                                 (uint16_t)CMD_Radar_sensitivity_set,
                                 &sensitivity, 1u);
    }
    co_printf("[DEBUG_SIM] FD cmd=0x%04X Success (0x00)\\r\\n", (unsigned)cmd);
    BleFunc_SendResultToRx(reply_cmd, 0x00);
#endif
}
