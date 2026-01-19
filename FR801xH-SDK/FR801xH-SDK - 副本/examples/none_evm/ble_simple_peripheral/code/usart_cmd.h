/**
 * @file    usart_cmd.h
 * @author  fzx-long (1456925916@qq.com)
 * @brief   USART 命令与 SOC↔MCU 帧协议公共定义
 * @version 0.2
 * @date    2026-01-04
 */

#ifndef USART_CMD_H
#define USART_CMD_H
/****************************Include***********************************/
#include <stdint.h>

/****************************Type Define*******************************/
#define SOC_MCU_FRAME_HEAD              (0xFEu)
#define SOC_MCU_FRAME_END0              (0x0Au)
#define SOC_MCU_FRAME_END1              (0x0Du)

#define SOC_MCU_SYNC_SOC_TO_MCU         (0xABu)
#define SOC_MCU_SYNC_MCU_TO_SOC         (0xBAu)

/* feature 值（按大端解析：字节序列 FF 01 表示 0xFF01） */
#define SOC_MCU_FEATURE_FF01            (0xFF01u)
#define SOC_MCU_FEATURE_FF02            (0xFF02u)
#define SOC_MCU_FEATURE_FF03            (0xFF03u)
#define SOC_MCU_FEATURE_FF04            (0xFF04u)

/* CRC 配置：1=CRC16(2B)，0=CRC8(1B)。如后续确认是 1B CRC，可切到 0。 */
#ifndef SOC_MCU_USE_CRC16
#define SOC_MCU_USE_CRC16               (1)
#endif

#if SOC_MCU_USE_CRC16
#define SOC_MCU_CRC_LEN                 (2u)
#else
#define SOC_MCU_CRC_LEN                 (1u)
#endif

/* 帧长度上限（含 head..end 全部字节）；需 <= UART RX buffer */
#ifndef SOC_MCU_FRAME_MAX_LEN
#define SOC_MCU_FRAME_MAX_LEN           (256u)
#endif

#define CMD_CONNECT          0x01  // 蓝牙连接命令 蓝牙连接后主动向MCU端发送(当蓝牙鉴权完成之后发送)
#define CMD_BLE_FACTORY_RESET 0x16  // 蓝牙恢复出厂设置命

/*取消蓝牙配对命令*/
#define CMD_BLE_CANCEL_PAIRING 0x17  // 取消蓝牙配对


/*NFC相关命令*/
#define CMD_ADD_NFC_KEY 0x20  // 添加NFC钥匙命令(基于不同的NFC模块进行定义)
#define CMD_DELETE_NFC_KEY 0x21  // 删除NFC钥匙命令
#define CMD_FIND_NFC_KEY 0x22  // 查找NFC钥匙命令
#define CMD_NFC_UNLOCK 0x23  // NFC解锁命令
#define CMD_NFC_LOCK 0x24  // NFC锁车命令
#define CMD_NFC_SWITCH 0x25  // NFC开关命令(开启或关闭NFC功能 00-关闭 01-开启)


/*油车解防命令*/
#define CMD_OIL_VEHICLE_UNPREVENTION 0x30  // 油车解防命令
#define CMD_OIL_VEHICLE_PREVENTION 0x31  // 油车防盗命令

/*油车寻车状态查询命令*/
#define CMD_OIL_VEHICLE_FIND_STATUS 0x32  // 油车寻车状态查询命令（这里会带DATA段，用于音量大小设置）

/*油车尾箱开关命令*/
#define CMD_OIL_VEHICLE_UNLOCK_TRUNK 0x33  // 油车尾箱解锁命令
#define CMD_OIL_VEHICLE_LOCK_TRUNK 0x34  // 油车尾箱锁车命令

/*座桶开关命令*/
#define CMD_VEHICLE_UNLOCK_SEAT 0x35  // 座桶解锁命令
#define CMD_VEHICLE_LOCK_SEAT 0x36  // 座桶锁车命令 

/*静音设置命令*/
#define CMD_VEHICLE_MUTE_SETTING 0x37  // 静音设置命令(DATA段 00-关闭静音 01-开启静音)

/*中箱锁开关命令*/
#define CMD_VEHICLE_UNLOCK_MIDDLE_BOX 0x38  // 中箱锁解锁命令
#define CMD_VEHICLE_LOCK_MIDDLE_BOX 0x39  // 中箱锁锁车命令

/*紧急模式命令*/
#define CMD_VEHICLE_EMERGENCY_MODE_LOCK 0x40  // 紧急模式打开
#define CMD_VEHICLE_EMERGENCY_MODE_UNLOCK 0x41  // 紧急模式关闭

/*MAC地址读取命令*/
#define CMD_BLE_MAC_READ 0x42  // 读取蓝牙MAC地址命

/*单控开锁命令*/
#define CMD_SINGLE_CONTROL_UNLOCK 0x43  // 单控开锁命令 尾箱锁



/*智能设置命令*/
/*助力推车模式设置命令*/
#define CMD_Assistive_trolley_mode_on 0x44  // 助力推车模式开启命令
#define CMD_Assistive_trolley_mode_off 0x45  // 助力推车模式关闭命令
#define CMD_Assistive_trolley_mode_default 0x46  // 默认助力推车模式命令

/*延时大灯设置命令*/
#define CMD_Delayed_headlight_on 0x47  // 延时大灯开启命令
#define CMD_Delayed_headlight_off 0x48  // 延时大灯关闭
#define CMD_Delayed_headlight_default 0x49  // 默认延时大灯命令
#define CMD_Delayed_headlight_time_set 0x50  // 延时大灯时间设置命令 DATA段 范围5-120秒 默认1

/*充电功率设置命令*/
#define CMD_Charging_power_set 0x51 // 充电功率设置命令 DATA段 档位：30 22 16 6 单位KW 默认30

/*自动P档设置命令*/
#define CMD_AUTO_P_GEAR_on 0x52  // 自动P档开启命令
#define CMD_AUTO_P_GEAR_off 0x53  // 自动P档关闭命令
#define CMD_AUTO_P_GEAR_default 0x54  // 自动P档默认命令
#define CMD_AUTO_P_GEAR_time_set 0x55  // 自动P档时间设置命令 DATA段 范围、10-120 单位s

/*和弦喇叭设置命令*/
#define CMD_Chord_horn_on 0x56  // 和弦喇
#define CMD_Chord_horn_off 0x57  // 和弦喇叭关闭命令
#define CMD_Chord_horn_default 0x58  // 和弦喇
#define CMD_Chord_horn_type_set 0x59  // 和弦喇叭音源设置 data段1
#define CMD_Chord_horn_volume_set 0x60  // 和弦喇叭音量设置 data段1

/*三射灯氛围灯设置命令*/
#define CMD_Three_shooting_lamp_on 0x61  // 三射灯氛围灯开启命令
#define CMD_Three_shooting_lamp_off 0x62  // 三射灯氛围灯关闭命令
#define CMD_Three_shooting_lamp_default 0x63  // 三射灯氛围灯默认命
#define CMD_Three_shooting_lamp_mode_set 0x64  // 三射灯氛围灯模式设置 data段 1字节灯光效果 1字节渐变模式 3字节颜色值

/*辅助倒车档位设置命令*/
#define CMD_Assistive_reversing_gear_on 0x65  // 辅助倒车档位开启命令
#define CMD_Assistive_reversing_gear_off 0x66  // 辅助倒车档位关闭命令
#define CMD_Assistive_reversing_gear_default 0x67  // 辅助倒车档位默认命令

/*自动转向回位设置命令*/
#define CMD_Automatic_steering_reset_on 0x75  // 自动转向回位开启命令
#define CMD_Automatic_steering_reset_off 0x76  // 自动转向回位关闭命令
#define CMD_Automatic_steering_reset_default 0x77  // 自动转向回位默认命令

/*EBS 开关设置命令*/
#define CMD_EBS_switch_on 0x78  // EBS 开启命令
/*data段：EBS类型 1字节*/

/*低速档位设置命令*/
#define CMD_Low_speed_gear_on 0x79  // 低速档位开启命令
#define CMD_Low_speed_gear_off 0x80  // 低速档位关闭命令
#define CMD_Low_speed_gear_default 0x81  // 低速档位默认命令
#define CMD_Low_speed_gear_speed_set 0x82  // 低速档位速度设置命令 00 01 10 11

/*中速档位设置命令*/
#define CMD_Medium_speed_gear_on 0x83  // 中速档位开启命令
#define CMD_Medium_speed_gear_off 0x84  // 中速档位关闭命令
#define CMD_Medium_speed_gear_default 0x85  // 中速档位默认命令
#define CMD_Medium_speed_gear_speed_set 0x86  // 中速档位速度设置命令 00 01 10 11
/*高速档位设置命令*/
#define CMD_High_speed_gear_on 0x87  // 高速档位开启命令
#define CMD_High_speed_gear_off 0x88  // 高速档位关闭命令
#define CMD_High_speed_gear_default 0x89  // 高速档位默认命令
#define CMD_High_speed_gear_speed_set 0x90  // 高速档位速度设置命令 00 01 10 11 

/*丢失模式设置命令*/
#define CMD_Lost_mode_on 0x91  // 丢失模式开启命令
#define CMD_Lost_mode_off 0x92  // 丢失模式关闭命令 默认

/*TCS开关设置命令*/
#define CMD_TCS_switch_on 0x93  // TCS 开启命令
#define CMD_TCS_switch_off 0x94  // TCS 关闭命令

/*侧支架开关设置命令*/
#define CMD_Side_stand_switch_on 0x95  // 侧支架开启命令
#define CMD_Side_stand_switch_off 0x96  // 侧支


/*电池模式设置命令*/
#define CMD_Battery_type_set 0x97  // 电池模式设置命令 DATA段 0x00-铅酸电池 0x01-锂电池
#define CMD_Battery_capacity_set 0x98  // 电池容量设置命令 DATA段 单位Ah


/*KFA、2KGA、2MQA数据同步命令*/
#define CMD_KFA_2KGA_2MQA_data_sync 0x99  // KFA、2KGA、2MQA数据同步命令 DATA段 根据不同车型定义

/*HDC开关设置命令*/
#define CMD_HDC_switch_on 0x100  // HDC 开启命令
#define CMD_HDC_switch_off 0x101  // HDC 关闭命令

/*HHC开关设置命令*/
#define CMD_HHC_switch_on 0x102  // HHC 开启命令
#define CMD_HHC_switch_off 0x103  // HHC 关闭命令

/*起步强度设置命令*/
#define CMD_Starting_strength_set 0x104  // 起步强度设置命令
/*DATA段 0x00-低 0x01-中 0x02-高 + 车类型*/

/*SPORT模式设置命令*/
#define CMD_SPORT_mode_on 0x105  // SPORT 模式开启命令
#define CMD_SPORT_mode_off 0x106  // SPORT 模式关闭命令
#define CMD_SPORT_mode_default 0x107  // SPORT 模式默认命令
#define CMD_SPORT_mode_type_set 0x108  // SPORT 模式类型设置命令 
/*范围30~65 单位Km/h 精度 偏移量 * + 车类型*/

/*ECO模式设置命令*/
#define CMD_ECO_mode_on 0x109  // ECO 模式开启命令
#define CMD_ECO_mode_off 0x110  // ECO 模式关闭命令
#define CMD_ECO_mode_default 0x111  // ECO 模式默认命令
#define CMD_ECO_mode_type_set 0x112  // ECO 模式类型设置命

/*雷达开关设置命令*/
#define CMD_Radar_switch_on 0x113  // 雷达开启命令
#define CMD_Radar_switch_off 0x114  // 雷达关闭命令
#define CMD_Radar_switch_default 0x115  // 雷达默认命令
//灵敏度设置
#define CMD_Radar_sensitivity_set 0x116  // 雷达灵敏度设置命令 DATA段 0x00-低 0x01-中 0x02-高

#define CMD_Tire_pressure_monitoring_get (0x0117u)// 读取胎压监测命令
#define CMD_BLE_RSSI_READ  (0x0118u)    // 读取蓝牙RSSI命令


#endif /* USART_CMD_H */
