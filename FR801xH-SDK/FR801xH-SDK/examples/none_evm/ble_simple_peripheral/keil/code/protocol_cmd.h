#ifndef PROTOCOL_CMD_H
#define PROTOCOL_CMD_H

// 定义协议命令ID   FE是APP标准的下行消息
#define connect_ID              0x01FE//连接命令
#define defences_ID             0x03FE//布防命令
#define anti_theft_ID           0x04FE//防盗命令
#define phone_message_ID        0x05FE//来电短信命令
#define riss_strength_ID        0x07FE//信号强度命令
#define car_search_ID           0x08FE//寻车命令
#define factory_settings_ID     0x09FE//恢复出厂设置命令
#define ble_unpair_ID           0x0AFE//解绑命令
#define add_nfc_ID              0x0BFE//添加NFC命令
#define delete_nfc_ID           0x0CFE//删除NFC命令
#define search_nfc_ID           0x0EFE//查询NFC命令
#define oil_defence_ID          0x0FFE//油电防护命令
#define oil_car_search_ID       0x11FE//油电寻车命令
#define set_boot_lock_ID        0x12FE//设置尾箱锁命令
#define set_nfc_ID              0x13FE//设置NFC命令lock
#define set_seat_lock_ID        0x14FE//设置座椅锁命令       
#define set_car_mute_ID         0x15FE//设置车辆静音命令
#define set_mid_box_lock_ID     0x16FE//设置中箱锁命令
#define set_emergency_mode_ID   0x17FE//紧急模式命令
#define get_phone_mac_ID        0x18FE//获取手机MAC地址命令
#define set_unlock_mode_ID      0x19FE//设置单控开锁 (有问题待和五羊本田确认)
#define charge_display_ID       0x1AFE//充电显示器开关 (0x01开/0x00关 -> MCU 0x200/0x201)


/* 智能协议类 FD是智能*/
#define assistive_trolley       0x02FD//智能助力推车
#define delayed_headlight       0x03FD//延时大灯 
#define set_charging_power      0x04FD//充电功率
#define set_p_gear_mode         0x05FD//设置P档模式
#define set_chord_horn_mode     0x06FD//设置和弦模式
#define set_RGB_light_mode      0x07FD//设置RGB氛围灯模式
#define set_auxiliary_parking   0x08FD//设置辅助倒车
#define set_intelligent_switch  0x63FD//设置智能开关
#define paramter_synchronize    0x64FD//参数同步(每次蓝牙连接上了都要同步一次)
#define paramter_synchronize_change 0x66FD//参数同步变化(参数变化后立即同步)
#define set_default_mode        0x65FD//设置默认模式
#define set_vichle_gurd_mode    0x09FD//设置车辆防盗
#define set_auto_return_mode    0x0aFD//设置自动归位
#define set_EBS_switch          0x0bFD//设置EBS开关
#define set_E_SAVE_mode         0x0cFD//设置E节能模式(低速挡)
#define set_DYN_mode            0x0dFD//设置DYN模式(中速档)
#define set_sport_mode          0x0eFD//设置SPORT模式(高速档)
#define set_lost_mode           0x0fFD//设置丢失模式
#define set_TCS_switch          0x10FD//设置TCS开关
#define set_side_stand          0x11FD//设置侧撑开关
#define set_battery_parameter   0x12FD//设置电池参数

#define set_updata_APP          0x13FD//数据同步(每次蓝牙连接上了，主动向app发送，保持连接，每隔30秒发送一次)
#define set_HDC_mode            0x14FD//设置HDC模式
#define set_HHC_mode            0x15FD//设置HHC模式
#define set_start_ability       0x16FD//设置启动能力
#define set_sport_power_speed         0x17FD//设置sport动力速度
#define set_ECO_mode           0x18FD//设置ECO模式
#define set_radar_switch       0x19FD//设置雷达开关

// 设备 -> 手机：鉴权结果回复（Connect 0x01FE 的应答）
#define auth_result_ID         0x0101

#endif // PROTOCOL_CMD_H
