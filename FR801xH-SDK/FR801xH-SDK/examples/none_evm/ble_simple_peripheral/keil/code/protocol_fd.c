#include "protocol_fd.h"
#include "protocol_cmd.h"
#include "ble_function.h"
#include "co_printf.h"

typedef void (*fd_cmd_handler_t)(uint16_t cmd, const uint8_t* payload, uint8_t len);

typedef struct
{
    uint16_t         cmd;
    fd_cmd_handler_t handler;
} fd_cmd_entry_t;

static const fd_cmd_entry_t s_fd_cmd_table[] = {
    {assistive_trolley, BleFunc_FD_AssistiveTrolley},
    {delayed_headlight, BleFunc_FD_DelayedHeadlight},
    {set_charging_power, BleFunc_FD_SetChargingPower},
    {set_p_gear_mode, BleFunc_FD_SetPGearMode},
    {set_chord_horn_mode, BleFunc_FD_SetChordHornMode},
    {set_RGB_light_mode, BleFunc_FD_SetRgbLightMode},
    {set_auxiliary_parking, BleFunc_FD_SetAuxiliaryParking},
    {set_intelligent_switch, BleFunc_FD_SetIntelligentSwitch},
    {paramter_synchronize, BleFunc_FD_ParamSynchronize},
    {paramter_synchronize_change, BleFunc_FD_ParamSynchronizeChange},
    {set_default_mode, BleFunc_FD_SetDefaultMode},
    {set_vichle_gurd_mode, BleFunc_FD_SetVichleGurdMode},
    {set_auto_return_mode, BleFunc_FD_SetAutoReturnMode},
    {set_EBS_switch, BleFunc_FD_SetEbsSwitch},
    {set_E_SAVE_mode, BleFunc_FD_SetESaveMode},
    {set_DYN_mode, BleFunc_FD_SetDynMode},
    {set_sport_mode, BleFunc_FD_SetSportMode},
    {set_lost_mode, BleFunc_FD_SetLostMode},
    {set_TCS_switch, BleFunc_FD_SetTcsSwitch},
    {set_side_stand, BleFunc_FD_SetSideStand},
    {set_battery_parameter, BleFunc_FD_SetBatteryParameter},
    {set_updata_APP, BleFunc_FD_SetUpdataApp},
    {set_HDC_mode, BleFunc_FD_SetHdcMode},
    {set_HHC_mode, BleFunc_FD_SetHhcMode},
    {set_start_ability, BleFunc_FD_SetStartAbility},
    {set_sport_power_speed, BleFunc_FD_SetSportPowerSpeed},
    {set_ECO_mode, BleFunc_FD_SetEcoMode},
    {set_radar_switch, BleFunc_FD_SetRadarSwitch},
};

void Protocol_Process_FD(uint16_t cmd, uint8_t* payload, uint8_t len) {
    co_printf("Protocol: Processing FD Cmd 0x%04X\r\n", cmd);

    for (uint32_t i = 0; i < (uint32_t)(sizeof(s_fd_cmd_table) / sizeof(s_fd_cmd_table[0])); i++) {
        if (s_fd_cmd_table[i].cmd != cmd) {
            continue;
        }
        s_fd_cmd_table[i].handler(cmd, (const uint8_t*)payload, len);
        return;
    }

    co_printf("  -> Unknown FD Cmd: 0x%04X\r\n", cmd);
}
