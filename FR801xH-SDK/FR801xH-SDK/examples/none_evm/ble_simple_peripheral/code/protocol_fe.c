#include "protocol_fe.h"
#include "protocol_cmd.h"
#include "ble_function.h"
#include "co_printf.h"

#include <stddef.h>

typedef void (*fe_cmd_handler_t)(uint16_t cmd, const uint8_t* payload, uint8_t len);

typedef struct
{
    uint16_t cmd;
    fe_cmd_handler_t handler;
} fe_cmd_entry_t;

static const fe_cmd_entry_t s_fe_cmd_table[] = {
    { connect_ID,            BleFunc_FE_Connect },
    { defences_ID,           BleFunc_FE_Defences },
    { anti_theft_ID,         BleFunc_FE_AntiTheft },
    { phone_message_ID,      BleFunc_FE_PhoneMessage },
    { riss_strength_ID,      BleFunc_FE_RssiStrength },
    { car_search_ID,         BleFunc_FE_CarSearch },
    { factory_settings_ID,   BleFunc_FE_FactorySettings },
    { ble_unpair_ID,         BleFunc_FE_BleUnpair },
    { add_nfc_ID,            BleFunc_FE_AddNfc },
    { delete_nfc_ID,         BleFunc_FE_DeleteNfc },
    { search_nfc_ID,         BleFunc_FE_SearchNfc },
    { oil_defence_ID,        BleFunc_FE_OilDefence },
    { oil_car_search_ID,     BleFunc_FE_OilCarSearch },
    { set_boot_lock_ID,      BleFunc_FE_SetBootLock },
    { set_nfc_ID,            BleFunc_FE_SetNfc },
    { set_seat_lock_ID,      BleFunc_FE_SetSeatLock },
    { set_car_mute_ID,       BleFunc_FE_SetCarMute },
    { set_mid_box_lock_ID,   BleFunc_FE_SetMidBoxLock },
    { set_emergency_mode_ID, BleFunc_FE_SetEmergencyMode },
    { get_phone_mac_ID,      BleFunc_FE_GetPhoneMac },
    { set_unlock_mode_ID,    BleFunc_FE_SetUnlockMode },
    { charge_display_ID,     BleFunc_FE_ChargeDisplay },
};

void Protocol_Process_FE(uint16_t cmd, uint8_t* payload, uint8_t len)
{
    co_printf("Protocol: Processing FE Cmd 0x%04X\r\n", cmd);

    for (uint32_t i = 0; i < (uint32_t)(sizeof(s_fe_cmd_table) / sizeof(s_fe_cmd_table[0])); i++)
    {
        if (s_fe_cmd_table[i].cmd != cmd)
        {
            continue;
        }

        // handler 在表里保证非空
        s_fe_cmd_table[i].handler(cmd, (const uint8_t*)payload, len);
        return;
    }

    co_printf("  -> Unknown FE Cmd: 0x%04X\r\n", cmd);
}
