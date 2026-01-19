/**
 * @file    usart_cmd.h
 * @brief   USART command definitions for SOC<->MCU frames.
 */

#ifndef USART_CMD_H
#define USART_CMD_H
/****************************Include***********************************/
#include <stdint.h>

/****************************Type Define*******************************/
#define SOC_MCU_FRAME_HEAD (0xFEu)
#define SOC_MCU_FRAME_END0 (0x0Au)
#define SOC_MCU_FRAME_END1 (0x0Du)

#define SOC_MCU_SYNC_SOC_TO_MCU (0xABu)
#define SOC_MCU_SYNC_MCU_TO_SOC (0xBAu)

/* feature values (big-endian: FF 01 => 0xFF01) */
#define SOC_MCU_FEATURE_FF01 (0xFF01u)
#define SOC_MCU_FEATURE_FF02 (0xFF02u)
#define SOC_MCU_FEATURE_FF03 (0xFF03u)
#define SOC_MCU_FEATURE_FF04 (0xFF04u)

/* checksum config: use 1B BinComplementCheckSum */
#ifndef SOC_MCU_USE_CRC16
#define SOC_MCU_USE_CRC16 (0)
#endif

#if SOC_MCU_USE_CRC16
#define SOC_MCU_CRC_LEN (2u)
#else
#define SOC_MCU_CRC_LEN (1u)
#endif

/* frame length max (head..end) */
#ifndef SOC_MCU_FRAME_MAX_LEN
#define SOC_MCU_FRAME_MAX_LEN (256u)
#endif

#define CMD_CONNECT            0x01 // BLE connect
#define CMD_BLE_FACTORY_RESET  0x16 // BLE factory reset
#define CMD_BLE_CANCEL_PAIRING 0x17 // BLE cancel pairing

/* NFC */
#define CMD_ADD_NFC_KEY    0x20
#define CMD_DELETE_NFC_KEY 0x21
#define CMD_FIND_NFC_KEY   0x22
#define CMD_NFC_UNLOCK     0x23
#define CMD_NFC_LOCK       0x24
#define CMD_NFC_SWITCH     0x25 // 00 off, 01 on

/* Oil vehicle */
#define CMD_OIL_VEHICLE_UNPREVENTION      0x30
#define CMD_OIL_VEHICLE_PREVENTION        0x31
#define CMD_OIL_VEHICLE_FIND_STATUS       0x32
#define CMD_OIL_VEHICLE_UNLOCK_TRUNK      0x33
#define CMD_OIL_VEHICLE_LOCK_TRUNK        0x34
#define CMD_VEHICLE_UNLOCK_SEAT           0x35
#define CMD_VEHICLE_LOCK_SEAT             0x36
#define CMD_VEHICLE_MUTE_SETTING          0x37
#define CMD_VEHICLE_UNLOCK_MIDDLE_BOX     0x38
#define CMD_VEHICLE_LOCK_MIDDLE_BOX       0x39
#define CMD_VEHICLE_EMERGENCY_MODE_LOCK   0x40
#define CMD_VEHICLE_EMERGENCY_MODE_UNLOCK 0x41

/* BLE MAC */
#define CMD_BLE_MAC_READ          0x42
#define CMD_SINGLE_CONTROL_UNLOCK 0x43

/* Intelligent settings */
#define CMD_Assistive_trolley_mode_on      0x44
#define CMD_Assistive_trolley_mode_off     0x45
#define CMD_Assistive_trolley_mode_default 0x46

#define CMD_Delayed_headlight_on       0x47
#define CMD_Delayed_headlight_off      0x48
#define CMD_Delayed_headlight_default  0x49
#define CMD_Delayed_headlight_time_set 0x50

#define CMD_Charging_power_set 0x51

#define CMD_AUTO_P_GEAR_on       0x52
#define CMD_AUTO_P_GEAR_off      0x53
#define CMD_AUTO_P_GEAR_default  0x54
#define CMD_AUTO_P_GEAR_time_set 0x55

#define CMD_Chord_horn_on         0x56
#define CMD_Chord_horn_off        0x57
#define CMD_Chord_horn_default    0x58
#define CMD_Chord_horn_type_set   0x59
#define CMD_Chord_horn_volume_set 0x60

#define CMD_Three_shooting_lamp_on       0x61
#define CMD_Three_shooting_lamp_off      0x62
#define CMD_Three_shooting_lamp_default  0x63
#define CMD_Three_shooting_lamp_mode_set 0x64

#define CMD_Assistive_reversing_gear_on      0x65
#define CMD_Assistive_reversing_gear_off     0x66
#define CMD_Assistive_reversing_gear_default 0x67

#define CMD_Automatic_steering_reset_on      0x75
#define CMD_Automatic_steering_reset_off     0x76
#define CMD_Automatic_steering_reset_default 0x77

#define CMD_EBS_switch_on 0x78

#define CMD_Low_speed_gear_on        0x79
#define CMD_Low_speed_gear_off       0x80
#define CMD_Low_speed_gear_default   0x81
#define CMD_Low_speed_gear_speed_set 0x82

#define CMD_Medium_speed_gear_on        0x83
#define CMD_Medium_speed_gear_off       0x84
#define CMD_Medium_speed_gear_default   0x85
#define CMD_Medium_speed_gear_speed_set 0x86

#define CMD_High_speed_gear_on        0x87
#define CMD_High_speed_gear_off       0x88
#define CMD_High_speed_gear_default   0x89
#define CMD_High_speed_gear_speed_set 0x90

#define CMD_Lost_mode_on  0x91
#define CMD_Lost_mode_off 0x92

#define CMD_TCS_switch_on  0x93
#define CMD_TCS_switch_off 0x94

#define CMD_Side_stand_switch_on  0x95
#define CMD_Side_stand_switch_off 0x96

#define CMD_Battery_type_set     0x97
#define CMD_Battery_capacity_set 0x98

#define CMD_KFA_2KGA_2MQA_data_sync 0x99

#define CMD_HDC_switch_on  0x100
#define CMD_HDC_switch_off 0x101

#define CMD_HHC_switch_on  0x102
#define CMD_HHC_switch_off 0x103

#define CMD_Starting_strength_set 0x104

#define CMD_SPORT_mode_on       0x105
#define CMD_SPORT_mode_off      0x106
#define CMD_SPORT_mode_default  0x107
#define CMD_SPORT_mode_type_set 0x108

#define CMD_ECO_mode_on       0x109
#define CMD_ECO_mode_off      0x110
#define CMD_ECO_mode_default  0x111
#define CMD_ECO_mode_type_set 0x112

/* Power recover (EBS extension) */
#define CMD_POWER_RECOVER_ON  0x202
#define CMD_POWER_RECOVER_OFF 0x203

/* Intelligent switch extensions */
#define CMD_ABS_ON_OFF            0x204 // DATA: 00=off 01=on
#define CMD_POWER_LED             0x205 // DATA: 00=off 01=on
#define CMD_SADDLE_ON_OFF         0x206 // DATA: 00=off 01=on
#define CMD_Position_light_on_off 0x207 // DATA: 00=off 01=on

/* Radar */
#define CMD_Radar_switch_on       0x113
#define CMD_Radar_switch_off      0x114
#define CMD_Radar_switch_default  0x115
#define CMD_Radar_sensitivity_set 0x116 // 0=low 1=mid 2=high

#define CMD_Tire_pressure_monitoring_get (0x0117u)
#define CMD_BLE_RSSI_READ                (0x0118u)
#define CMD_BLE_RSSI_RANGE_SET           (0x0119u)

/* Charge display */
#define CMD_CHARGE_DISPLAY_ON  (0x0200u)
#define CMD_CHARGE_DISPLAY_OFF (0x0201u)

/*ble Rssi lock*/
#define CMD_BLE_RSSI_LOCK_ON  (0x0120u)
#define CMD_BLE_RSSI_LOCK_OFF (0x0121u)

#endif /* USART_CMD_H */
