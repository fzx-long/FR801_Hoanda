#include "param_sync.h"

#include "protocol.h"

#include "protocol_cmd.h"

#include "co_printf.h"

#include <string.h>
#include "gap_api.h"

/*

 * 说明�?

 * - 0x64FD �?payload 字段非常多，且文档后半段字段在你发的片段里存在“排�?含义不完整”的情况�?

 * - 为了保证 APP 至少能收到“结构正确、长度稳定”的同步帧，这里先按已知字段顺序组包�?

 * - 未明确的字段/保留字段默认�?0x00�?

 *

 * 你后续如果确认了字段长度/顺序，只需要修改这里的打包顺序即可，不用动 ble_function.c�?

 */

/* 6.10�?x64FD payload（按你提供的表格顺序，单位：byte�?*/

typedef struct {

    // uint8_t userControl;

    // uint8_t userSpeed;

    // uint8_t userMaxTorque;

    uint8_t boosterCartControl;

    uint8_t boosterSpeed;

    uint8_t delayedHeadlightsControl;

    uint8_t delayedHeadlightsDelay;

    uint8_t chargingPower;

    uint8_t autoShiftToPControl;

    uint8_t autoShiftToPDelay;

    uint8_t chordHornSoundSource;

    uint8_t chordHornVolume;

    uint8_t lightingEffects;

    uint8_t gradual;

    uint8_t changliangRgbr;

    uint8_t changliangRgbg;

    uint8_t changliangRgbb;

    uint8_t breatheRgbr;

    uint8_t breatheRgbg;

    uint8_t breatheRgbb;

    uint8_t marqueeRgbr;

    uint8_t marqueeRgbg;

    uint8_t marqueeRgbb;

    uint8_t intelligentSwitch1;

    uint8_t intelligentSwitch2;

    uint8_t reverseControl;

    uint8_t reverseSpeed;

    uint8_t isAlarmArmed;

    uint8_t alarmSensitivity;

    uint8_t alarmVolume;

    uint8_t AutoTurnResetSettings;

    uint8_t ebsSettings;

    uint8_t eSaveModeSettings;

    uint8_t dynModeSettings;

    uint8_t sportModeSettings;

    uint8_t intelligentSwitch3;

    uint8_t intelligentSwitch4;

} param_sync_64fd_t;

static param_sync_64fd_t s_64fd_cache;

static uint8_t s_64fd_valid = 0u;

static uint8_t s_64fd_pending_mask = 0u;

#define PARAM_SYNC_64FD_LEN (43u)

/* 6.11�?x66FD payload = intelligentSwitch1(车辆状�?1byte) + speed(2byte, 0.1km/h) */

static uint8_t s_66fd_vehicle_status;

static uint16_t s_66fd_speed_01kmh;

/*

 * 为什么不�?Protocol_Send_Unicast�?

 * - Protocol_Send_Unicast 会优先复�?last_rx_seq（为了“应答回�?seq”）�?

 * - 0x64FD/0x66FD 属于设备主动推送，不应复用上一个请�?seq，否�?APP 可能把它当成上一个请求的应答而丢弃�?

 *

 * 因此这里使用 Protocol_Send_Unicast_Async（新加的接口），强制用新�?seq�?

 */

static void ParamSync_Send64FD(uint8_t conidx)

{

    uint8_t payload[64];

    uint16_t i = 0;

    memset(payload, 0, sizeof(payload));

    /* 按结构体顺序逐字节打包，确保 payload 顺序稳定 */

    // payload[i++] = s_64fd_cache.userControl;

    // payload[i++] = s_64fd_cache.userSpeed;

    // payload[i++] = s_64fd_cache.userMaxTorque;

    payload[i++] = s_64fd_cache.boosterCartControl;

    payload[i++] = s_64fd_cache.boosterSpeed;

    payload[i++] = s_64fd_cache.delayedHeadlightsControl;

    payload[i++] = s_64fd_cache.delayedHeadlightsDelay;

    payload[i++] = s_64fd_cache.chargingPower;

    payload[i++] = s_64fd_cache.autoShiftToPControl;

    payload[i++] = s_64fd_cache.autoShiftToPDelay;

    payload[i++] = s_64fd_cache.chordHornSoundSource;

    payload[i++] = s_64fd_cache.chordHornVolume;

    payload[i++] = s_64fd_cache.lightingEffects;

    payload[i++] = s_64fd_cache.gradual;

    payload[i++] = s_64fd_cache.changliangRgbr;

    payload[i++] = s_64fd_cache.changliangRgbg;

    payload[i++] = s_64fd_cache.changliangRgbb;

    payload[i++] = s_64fd_cache.breatheRgbr;

    payload[i++] = s_64fd_cache.breatheRgbg;

    payload[i++] = s_64fd_cache.breatheRgbb;

    payload[i++] = s_64fd_cache.marqueeRgbr;

    payload[i++] = s_64fd_cache.marqueeRgbg;

    payload[i++] = s_64fd_cache.marqueeRgbb;

    payload[i++] = s_64fd_cache.intelligentSwitch1;

    payload[i++] = s_64fd_cache.intelligentSwitch2;

    payload[i++] = s_64fd_cache.reverseControl;

    payload[i++] = s_64fd_cache.reverseSpeed;

    payload[i++] = s_64fd_cache.isAlarmArmed;

    payload[i++] = s_64fd_cache.alarmSensitivity;

    payload[i++] = s_64fd_cache.alarmVolume;

    payload[i++] = s_64fd_cache.AutoTurnResetSettings;

    payload[i++] = s_64fd_cache.ebsSettings;

    payload[i++] = s_64fd_cache.eSaveModeSettings;

    payload[i++] = s_64fd_cache.dynModeSettings;

    payload[i++] = s_64fd_cache.sportModeSettings;

    payload[i++] = s_64fd_cache.intelligentSwitch3;

    payload[i++] = s_64fd_cache.intelligentSwitch4;

    // payload[i++] = s_64fd_cache.ecoSpeedSetting;

    // payload[i++] = s_64fd_cache.startStrengthSetting;

    // payload[i++] = (uint8_t)(s_64fd_cache.bleUnlockSensitivity & 0xFFu);

    // payload[i++] = (uint8_t)((s_64fd_cache.bleUnlockSensitivity >> 8) & 0xFFu);

    // payload[i++] = s_64fd_cache.sportSpeedSetting;

    // payload[i++] = s_64fd_cache.ebsStrength;

    co_printf("[PARAM_SYNC] TX 0x64FD conidx=%u len=%u\\r\\n",
              (unsigned)conidx,
              (unsigned)i);

    (void)Protocol_Send_Unicast_Async(conidx, paramter_synchronize, payload, i);
}

static void ParamSync_Send66FD(uint8_t conidx)

{

    uint8_t payload[3];

    payload[0] = s_66fd_vehicle_status;

    payload[1] = (uint8_t)(s_66fd_speed_01kmh & 0xFFu);

    payload[2] = (uint8_t)((s_66fd_speed_01kmh >> 8) & 0xFFu);

    co_printf("[PARAM_SYNC] TX 0x66FD conidx=%u status=0x%02X "
              "speed=%u(0.1km/h)\\r\\n",
              (unsigned)conidx,
              (unsigned)payload[0],
              (unsigned)s_66fd_speed_01kmh);

    (void)Protocol_Send_Unicast_Async(conidx,
                                      paramter_synchronize_change,
                                      payload,
                                      (uint16_t)sizeof(payload));
}

static bool ParamSync_Update64FD(const uint8_t* data, uint16_t len)

{

    if (data == NULL) {

        return false;
    }

    uint16_t i = 0;

    if (len == (uint16_t)(PARAM_SYNC_64FD_LEN + 2u)) {

        i = 2u;

    } else if (len < PARAM_SYNC_64FD_LEN) {

        return false;
    }

    // s_64fd_cache.userControl = data[i++];

    // s_64fd_cache.userSpeed = data[i++];

    // s_64fd_cache.userMaxTorque = data[i++];

    s_64fd_cache.boosterCartControl = data[i++];

    s_64fd_cache.boosterSpeed = data[i++];

    s_64fd_cache.delayedHeadlightsControl = data[i++];

    s_64fd_cache.delayedHeadlightsDelay = data[i++];

    s_64fd_cache.chargingPower = data[i++];

    s_64fd_cache.autoShiftToPControl = data[i++];

    s_64fd_cache.autoShiftToPDelay = data[i++];

    s_64fd_cache.chordHornSoundSource = data[i++];

    s_64fd_cache.chordHornVolume = data[i++];

    s_64fd_cache.lightingEffects = data[i++];

    s_64fd_cache.gradual = data[i++];

    s_64fd_cache.changliangRgbr = data[i++];

    s_64fd_cache.changliangRgbg = data[i++];

    s_64fd_cache.changliangRgbb = data[i++];

    s_64fd_cache.breatheRgbr = data[i++];

    s_64fd_cache.breatheRgbg = data[i++];

    s_64fd_cache.breatheRgbb = data[i++];

    s_64fd_cache.marqueeRgbr = data[i++];

    s_64fd_cache.marqueeRgbg = data[i++];

    s_64fd_cache.marqueeRgbb = data[i++];

    s_64fd_cache.intelligentSwitch1 = data[i++];

    s_64fd_cache.intelligentSwitch2 = data[i++];

    s_64fd_cache.reverseControl = data[i++];

    s_64fd_cache.reverseSpeed = data[i++];

    s_64fd_cache.isAlarmArmed = data[i++];

    s_64fd_cache.alarmSensitivity = data[i++];

    s_64fd_cache.alarmVolume = data[i++];

    s_64fd_cache.AutoTurnResetSettings = data[i++];

    s_64fd_cache.ebsSettings = data[i++];

    s_64fd_cache.eSaveModeSettings = data[i++];

    s_64fd_cache.dynModeSettings = data[i++];

    s_64fd_cache.sportModeSettings = data[i++];

    s_64fd_cache.intelligentSwitch3 = data[i++];

    s_64fd_cache.intelligentSwitch4 = data[i++];

    // s_64fd_cache.ecoSpeedSetting = data[i++];

    // s_64fd_cache.startStrengthSetting = data[i++];

    // s_64fd_cache.bleUnlockSensitivity =
    //     (uint16_t)((uint16_t)data[i] | ((uint16_t)data[i + 1] << 8));

    // i = (uint16_t)(i + 2u);

    // s_64fd_cache.sportSpeedSetting = data[i++];

    // s_64fd_cache.ebsStrength = data[i++];

    s_64fd_valid = 1u;

    return true;
}

void ParamSync_OnMcuSync64FD(const uint8_t* data, uint16_t len)

{

    if (!ParamSync_Update64FD(data, len)) {

        co_printf("[PARAM_SYNC] RX 0x0208 len=%u (short)\r\n", (unsigned)len);

        return;
    }

    co_printf("[PARAM_SYNC] RX 0x0208 len=%u\r\n", (unsigned)len);

    for (uint8_t conidx = 0; conidx < 3u; conidx++) {

        if (!Protocol_Auth_IsOk(conidx)) {

            continue;
        }

        if (gap_get_connect_status(conidx) == 0) {

            continue;
        }

        if ((s_64fd_pending_mask & (uint8_t)(1u << conidx)) != 0u) {

            ParamSync_Send64FD(conidx);

            s_64fd_pending_mask &=

                (uint8_t)~(uint8_t)(1u << conidx);
        }
    }
}

void ParamSync_OnBleAuthed(uint8_t conidx)

{

    /* ֻ�����������Ѽ�Ȩ conidx ��������δ��¼�����յ�ҵ��֡ */

    if (!Protocol_Auth_IsOk(conidx)) {

        return;
    }

    if (gap_get_connect_status(conidx) == 0) {

        return;
    }

    /* 0x64FD wait MCU 0x0208 then sync */

    if (s_64fd_valid != 0u) {

        ParamSync_Send64FD(conidx);

    } else {

        s_64fd_pending_mask |= (uint8_t)(1u << conidx);
    }

    ParamSync_Send66FD(conidx);
}

void ParamSync_NotifyChange(uint8_t  intelligentSwitch1_vehicleStatus,
                            uint16_t speed_01kmh)

{

    /* 更新缓存 */

    s_66fd_vehicle_status = intelligentSwitch1_vehicleStatus;

    s_66fd_speed_01kmh = speed_01kmh;

    /* 变化同步：推给所有已鉴权连接 */

    for (uint8_t conidx = 0; conidx < 3u; conidx++) {

        if (!Protocol_Auth_IsOk(conidx)) {

            continue;
        }

        if (gap_get_connect_status(conidx) == 0) {

            continue;
        }

        ParamSync_Send66FD(conidx);
    }
}

void ParamSync_OnAppReply(uint8_t        conidx,
                          uint16_t       cmd,
                          const uint8_t* payload,
                          uint8_t        len)

{

    uint8_t rc = 0xFFu;

    if (payload != NULL && len >= 1u) {

        rc = payload[0];
    }

    if (cmd == (uint16_t)0x6402u) {

        co_printf("[PARAM_SYNC] RX 0x6402 conidx=%u rc=0x%02X\\r\\n",
                  (unsigned)conidx,
                  (unsigned)rc);

        return;
    }

    if (cmd == (uint16_t)0x6602u) {

        co_printf("[PARAM_SYNC] RX 0x6602 conidx=%u rc=0x%02X\\r\\n",
                  (unsigned)conidx,
                  (unsigned)rc);

        return;
    }

    /* 其他 0x??02 暂不处理，仅打日�?*/

    co_printf("[PARAM_SYNC] RX 0x%04X conidx=%u len=%u\\r\\n",
              (unsigned)cmd,
              (unsigned)conidx,
              (unsigned)len);
}
