#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模拟回复系统
用于模拟MCU的响应
"""

class ResponseSimulator:
    """
    响应模拟器
    """
    def __init__(self):
        self.response_templates = {
            # 蓝牙相关命令
            0x01: self._handle_connect,
            0x16: self._handle_factory_reset,
            0x17: self._handle_cancel_pairing,
            0x42: self._handle_read_mac,
            
            # NFC相关命令
            0x20: self._handle_add_nfc_key,
            0x21: self._handle_delete_nfc_key,
            0x22: self._handle_find_nfc_key,
            0x23: self._handle_nfc_unlock,
            0x24: self._handle_nfc_lock,
            0x25: self._handle_nfc_switch,
            
            # 车辆控制命令
            0x30: self._handle_oil_unprevent,
            0x31: self._handle_oil_prevent,
            0x32: self._handle_oil_find_status,
            0x33: self._handle_oil_unlock_trunk,
            0x34: self._handle_oil_lock_trunk,
            0x35: self._handle_unlock_seat,
            0x36: self._handle_lock_seat,
            0x37: self._handle_mute_setting,
            0x38: self._handle_unlock_middle_box,
            0x39: self._handle_lock_middle_box,
            0x40: self._handle_emergency_mode_on,
            0x41: self._handle_emergency_mode_off,
            0x43: self._handle_single_control_unlock,
            
            # 智能设置命令
            0x44: self._handle_assistive_trolley_on,
            0x45: self._handle_assistive_trolley_off,
            0x46: self._handle_assistive_trolley_default,
            0x47: self._handle_delayed_headlight_on,
            0x48: self._handle_delayed_headlight_off,
            0x49: self._handle_delayed_headlight_default,
            0x50: self._handle_delayed_headlight_time_set,
            0x51: self._handle_charging_power_set,
            0x52: self._handle_auto_p_gear_on,
            0x53: self._handle_auto_p_gear_off,
            0x54: self._handle_auto_p_gear_default,
            0x55: self._handle_auto_p_gear_time_set,
            0x56: self._handle_chord_horn_on,
            0x57: self._handle_chord_horn_off,
            0x58: self._handle_chord_horn_default,
            0x59: self._handle_chord_horn_type_set,
            0x60: self._handle_chord_horn_volume_set,
            0x61: self._handle_three_shooting_lamp_on,
            0x62: self._handle_three_shooting_lamp_off,
            0x63: self._handle_three_shooting_lamp_default,
            0x64: self._handle_three_shooting_lamp_mode_set,
            0x65: self._handle_assistive_reversing_gear_on,
            0x66: self._handle_assistive_reversing_gear_off,
            0x67: self._handle_assistive_reversing_gear_default,
            0x75: self._handle_automatic_steering_reset_on,
            0x76: self._handle_automatic_steering_reset_off,
            0x77: self._handle_automatic_steering_reset_default,
            0x78: self._handle_ebs_on,
            0x79: self._handle_low_speed_gear_on,
            0x80: self._handle_low_speed_gear_off,
            0x81: self._handle_low_speed_gear_default,
            0x82: self._handle_low_speed_gear_speed_set,
            0x83: self._handle_medium_speed_gear_on,
            0x84: self._handle_medium_speed_gear_off,
            0x85: self._handle_medium_speed_gear_default,
            0x86: self._handle_medium_speed_gear_speed_set,
            0x87: self._handle_high_speed_gear_on,
            0x88: self._handle_high_speed_gear_off,
            0x89: self._handle_high_speed_gear_default,
            0x90: self._handle_high_speed_gear_speed_set,
            0x91: self._handle_lost_mode_on,
            0x92: self._handle_lost_mode_off,
            0x93: self._handle_tcs_on,
            0x94: self._handle_tcs_off,
            0x95: self._handle_side_stand_on,
            0x96: self._handle_side_stand_off,
            0x97: self._handle_battery_type_set,
            0x98: self._handle_battery_capacity_set,
            0x99: self._handle_kfa_sync,
            0x100: self._handle_hdc_on,
            0x101: self._handle_hdc_off,
            0x102: self._handle_hhc_on,
            0x103: self._handle_hhc_off,
            0x104: self._handle_starting_strength_set,
            0x105: self._handle_sport_mode_on,
            0x106: self._handle_sport_mode_off,
            0x107: self._handle_sport_mode_default,
            0x108: self._handle_sport_mode_type_set,
            0x109: self._handle_eco_mode_on,
            0x110: self._handle_eco_mode_off,
            0x111: self._handle_eco_mode_default,
            0x112: self._handle_eco_mode_type_set,
            0x113: self._handle_radar_on,
            0x114: self._handle_radar_off,
            0x115: self._handle_radar_default,
            0x116: self._handle_radar_sensitivity_set,
            
            # 传感器相关命令
            0x117: self._handle_tire_pressure_get,
            0x11A: self._handle_tire_pressure_status_get,
            0x11B: self._handle_tire_pressure_pair,
            
            # 蓝牙RSSI命令
            0x118: self._handle_ble_rssi_read,
            0x119: self._handle_ble_rssi_sensitivity,
            0x120: self._handle_ble_rssi_on,
            0x121: self._handle_ble_rssi_off,
            
            # 防盗设置灵敏度命令
            0xA0: self._handle_theft_sensitivity_read,
            0xA1: self._handle_theft_sensitivity_set,
            
            # 充电显示器命令
            0x200: self._handle_charging_show_on,
            0x201: self._handle_charging_show_off,
            # 动力回收命令
            0x202: self._handle_power_recover_on,
            0x203: self._handle_power_recover_off,
            # 设备主动同步数据命令
            0x208: self._handle_device_sync_data,
            # 设备状态查询命令
            0x209: self._handle_device_status_query,
        }
    
    def get_response(self, cmd_id, data=b''):
        """
        获取模拟响应
        
        参数：
            cmd_id: 命令ID
            data: 命令数据
        
        返回：
            响应数据（字节）
        """
        handler = self.response_templates.get(cmd_id)
        if handler:
            return handler(data)
        else:
            return self._handle_default(cmd_id, data)
    
    def _handle_default(self, cmd_id, data):
        """
        默认处理
        """
        return b'\x00'  # 成功响应
    
    # 蓝牙相关命令处理
    def _handle_connect(self, data):
        """
        处理蓝牙连接命令
        """
        return b'\x00'  # 连接成功
    
    def _handle_factory_reset(self, data):
        """
        处理恢复出厂设置命令
        """
        return b'\x00'  # 重置成功
    
    def _handle_cancel_pairing(self, data):
        """
        处理取消配对命令
        """
        return b'\x00'  # 取消成功
    
    def _handle_read_mac(self, data):
        """
        处理读取MAC地址命令
        """
        return b'\x00\x11\x22\x33\x44\x55'  # 模拟MAC地址
    
    # NFC相关命令处理
    def _handle_add_nfc_key(self, data):
        """
        处理添加NFC钥匙命令
        """
        return b'\x00'  # 添加成功
    
    def _handle_delete_nfc_key(self, data):
        """
        处理删除NFC钥匙命令
        """
        return b'\x00'  # 删除成功
    
    def _handle_find_nfc_key(self, data):
        """
        处理查找NFC钥匙命令
        """
        return b'\x00\x02'  # 找到2个钥匙
    
    def _handle_nfc_unlock(self, data):
        """
        处理NFC解锁命令
        """
        return b'\x00'  # 解锁成功
    
    def _handle_nfc_lock(self, data):
        """
        处理NFC锁车命令
        """
        return b'\x00'  # 锁车成功
    
    def _handle_nfc_switch(self, data):
        """
        处理NFC开关命令
        """
        return b'\x00'  # 设置成功
    
    # 车辆控制命令处理
    def _handle_oil_unprevent(self, data):
        """
        处理油车解防命令
        """
        return b'\x00'  # 解防成功
    
    def _handle_oil_prevent(self, data):
        """
        处理油车防盗命令
        """
        return b'\x00'  # 设防成功
    
    def _handle_oil_find_status(self, data):
        """
        处理油车寻车状态查询命令
        """
        return b'\x00\x05'  # 寻车成功，音量5
    
    def _handle_oil_unlock_trunk(self, data):
        """
        处理油车尾箱解锁命令
        """
        return b'\x00'  # 解锁成功
    
    def _handle_oil_lock_trunk(self, data):
        """
        处理油车尾箱锁车命令
        """
        return b'\x00'  # 锁车成功
    
    def _handle_unlock_seat(self, data):
        """
        处理座桶解锁命令
        """
        return b'\x00'  # 解锁成功
    
    def _handle_lock_seat(self, data):
        """
        处理座桶锁车命令
        """
        return b'\x00'  # 锁车成功
    
    def _handle_mute_setting(self, data):
        """
        处理静音设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_unlock_middle_box(self, data):
        """
        处理中箱锁解锁命令
        """
        return b'\x00'  # 解锁成功
    
    def _handle_lock_middle_box(self, data):
        """
        处理中箱锁锁车命令
        """
        return b'\x00'  # 锁车成功
    
    def _handle_emergency_mode_on(self, data):
        """
        处理紧急模式打开命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_emergency_mode_off(self, data):
        """
        处理紧急模式关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_single_control_unlock(self, data):
        """
        处理单控开锁命令
        """
        return b'\x00'  # 解锁成功
    
    # 智能设置命令处理
    def _handle_assistive_trolley_on(self, data):
        """
        处理助力推车模式开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_assistive_trolley_off(self, data):
        """
        处理助力推车模式关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_assistive_trolley_default(self, data):
        """
        处理默认助力推车模式命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_delayed_headlight_on(self, data):
        """
        处理延时大灯开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_delayed_headlight_off(self, data):
        """
        处理延时大灯关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_delayed_headlight_default(self, data):
        """
        处理默认延时大灯命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_delayed_headlight_time_set(self, data):
        """
        处理延时大灯时间设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_charging_power_set(self, data):
        """
        处理充电功率设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_auto_p_gear_on(self, data):
        """
        处理自动P档开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_auto_p_gear_off(self, data):
        """
        处理自动P档关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_auto_p_gear_default(self, data):
        """
        处理自动P档默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_auto_p_gear_time_set(self, data):
        """
        处理自动P档时间设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_chord_horn_on(self, data):
        """
        处理和弦喇叭开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_chord_horn_off(self, data):
        """
        处理和弦喇叭关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_chord_horn_default(self, data):
        """
        处理和弦喇叭默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_chord_horn_type_set(self, data):
        """
        处理和弦喇叭音源设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_chord_horn_volume_set(self, data):
        """
        处理和弦喇叭音量设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_three_shooting_lamp_on(self, data):
        """
        处理三射灯氛围灯开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_three_shooting_lamp_off(self, data):
        """
        处理三射灯氛围灯关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_three_shooting_lamp_default(self, data):
        """
        处理三射灯氛围灯默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_three_shooting_lamp_mode_set(self, data):
        """
        处理三射灯氛围灯模式设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_assistive_reversing_gear_on(self, data):
        """
        处理辅助倒车档位开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_assistive_reversing_gear_off(self, data):
        """
        处理辅助倒车档位关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_assistive_reversing_gear_default(self, data):
        """
        处理辅助倒车档位默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_automatic_steering_reset_on(self, data):
        """
        处理自动转向回位开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_automatic_steering_reset_off(self, data):
        """
        处理自动转向回位关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_automatic_steering_reset_default(self, data):
        """
        处理自动转向回位默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_ebs_on(self, data):
        """
        处理EBS开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_low_speed_gear_on(self, data):
        """
        处理低速档位开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_low_speed_gear_off(self, data):
        """
        处理低速档位关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_low_speed_gear_default(self, data):
        """
        处理低速档位默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_low_speed_gear_speed_set(self, data):
        """
        处理低速档位速度设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_medium_speed_gear_on(self, data):
        """
        处理中速档位开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_medium_speed_gear_off(self, data):
        """
        处理中速档位关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_medium_speed_gear_default(self, data):
        """
        处理中速档位默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_medium_speed_gear_speed_set(self, data):
        """
        处理中速档位速度设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_high_speed_gear_on(self, data):
        """
        处理高速档位开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_high_speed_gear_off(self, data):
        """
        处理高速档位关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_high_speed_gear_default(self, data):
        """
        处理高速档位默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_high_speed_gear_speed_set(self, data):
        """
        处理高速档位速度设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_lost_mode_on(self, data):
        """
        处理丢失模式开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_lost_mode_off(self, data):
        """
        处理丢失模式关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_tcs_on(self, data):
        """
        处理TCS开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_tcs_off(self, data):
        """
        处理TCS关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_side_stand_on(self, data):
        """
        处理侧支架开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_side_stand_off(self, data):
        """
        处理侧支架关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_battery_type_set(self, data):
        """
        处理电池模式设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_battery_capacity_set(self, data):
        """
        处理电池容量设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_kfa_sync(self, data):
        """
        处理KFA数据同步命令
        """
        return b'\x00'  # 同步成功
    
    def _handle_hdc_on(self, data):
        """
        处理HDC开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_hdc_off(self, data):
        """
        处理HDC关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_hhc_on(self, data):
        """
        处理HHC开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_hhc_off(self, data):
        """
        处理HHC关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_starting_strength_set(self, data):
        """
        处理起步强度设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_sport_mode_on(self, data):
        """
        处理SPORT模式开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_sport_mode_off(self, data):
        """
        处理SPORT模式关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_sport_mode_default(self, data):
        """
        处理SPORT模式默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_sport_mode_type_set(self, data):
        """
        处理SPORT模式类型设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_eco_mode_on(self, data):
        """
        处理ECO模式开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_eco_mode_off(self, data):
        """
        处理ECO模式关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_eco_mode_default(self, data):
        """
        处理ECO模式默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_eco_mode_type_set(self, data):
        """
        处理ECO模式类型设置命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_radar_on(self, data):
        """
        处理雷达开启命令
        """
        return b'\x00'  # 开启成功
    
    def _handle_radar_off(self, data):
        """
        处理雷达关闭命令
        """
        return b'\x00'  # 关闭成功
    
    def _handle_radar_default(self, data):
        """
        处理雷达默认命令
        """
        return b'\x00'  # 设置成功
    
    def _handle_radar_sensitivity_set(self, data):
        """
        处理雷达灵敏度设置命令
        """
        return b'\x00'  # 设置成功
    
    # 传感器相关命令处理
    def _handle_tire_pressure_get(self, data):
        """
        处理胎压监测获取命令
        """
        # 模拟胎压数据：前左、前右、后左、后右
        return b'\x30\x32\x30\x32'  # 2.0 bar
    
    def _handle_tire_pressure_delete(self, data):
        """
        处理胎压传感器删除命令
        """
        return b'\x00'  # 删除成功
    
    def _handle_tire_pressure_pair(self, data):
        """
        处理胎压传感器配对命令
        """
        return b'\x00'  # 配对成功
    
    def _handle_tire_pressure_status_get(self, data):
        """
        处理胎压传感器状态获取命令
        """
        # 模拟状态数据：正常
        return b'\x00\x00\x00\x00'  # 四个轮胎都正常
    
    # 防盗设置灵敏度命令处理
    def _handle_theft_sensitivity_read(self, data):
        """
        处理防盗设置灵敏度读取命令
        """
        # 模拟返回当前灵敏度设置：中
        return b'\x00\x01'  # 成功，当前灵敏度为中
    
    def _handle_theft_sensitivity_set(self, data):
        """
        处理防盗设置灵敏度设置命令
        """
        return b'\x00'  # 设置成功
    
    # 蓝牙RSSI命令处理
    def _handle_ble_rssi_read(self, data):
        """
        处理读取蓝牙RSSI值命令
        """
        # 模拟返回RSSI值和MAC地址
        # RSSI值：0x7F（模拟值）
        # MAC地址：11:22:33:44:55:66
        return b'\x7F\x11\x22\x33\x44\x55\x66'  # RSSI值1B + MAC地址6B

    def _handle_ble_rssi_sensitivity(self, data):
        """
        处理蓝牙无感解锁灵敏度命令
        """
        return b'\x00'  # 设置成功

    def _handle_ble_rssi_on(self, data):
        """
        处理蓝牙无感解锁开命令
        """
        return b'\x00'  # 开启成功

    def _handle_ble_rssi_off(self, data):
        """
        处理蓝牙无感解锁关命令
        """
        return b'\x00'  # 关闭成功

    def _handle_charging_show_on(self, data):
        """
        处理充电显示器开命令
        """
        return b'\x00'  # 开启成功

    def _handle_charging_show_off(self, data):
        """
        处理充电显示器关命令
        """
        return b'\x00'  # 关闭成功

    def _handle_power_recover_on(self, data):
        """
        处理动力回收开命令(EBS)
        """
        return b'\x00'  # 开启成功

    def _handle_power_recover_off(self, data):
        """
        处理动力回收关命令(EBS)
        """
        return b'\x00'  # 关闭成功

    def _handle_device_status_query(self, data):
        """
        处理设备状态查询命令
        返回设备当前状态信息
        """
        # 构建设备状态字节
        # 设备状态: 正常运行
        device_status = 0x00  # 0=正常
        
        # 电池状态: 正常
        battery_status = 0x00  # 0=正常
        
        # 蓝牙状态: 已连接
        bluetooth_status = 0x01  # 1=已连接
        
        # GPS状态: 已定位
        gps_status = 0x01  # 1=已定位
        
        # 构建响应数据
        response_data = bytes([
            device_status,
            battery_status,
            bluetooth_status,
            gps_status
        ])
        
        return response_data

    def _handle_device_sync_data(self, data):
        """
        处理设备主动同步数据命令
        按照蓝牙协议格式返回数据：
        每次蓝牙连接上后，设备主动向APP同步一次
        """
        # 构建响应数据，按照蓝牙协议格式
        response_data = bytes([
            # User档开关
            0x01,  # User档开关: 开
            # User档速度
            0x3C,  # User档速度: 60
            # User档扭矩
            0x64,  # User档扭矩: 100
            # 助力推车开关
            0x01,  # 助力推车开关: 开
            # 助力推车速度
            0x0A,  # 助力推车速度: 10
            # 延时大灯开关
            0x01,  # 延时大灯开关: 开
            # 延时大灯延时时间
            0x1E,  # 延时大灯延时时间: 30秒
            # 充电功率
            0x1E,  # 充电功率: 30
            # P档开关
            0x01,  # P档开关: 开
            # P档延时时间
            0x0A,  # P档延时时间: 10秒
            # 和弦喇叭音源
            0x01,  # 和弦喇叭音源: 1
            # 和弦喇叭音量大小
            0x0A,  # 和弦喇叭音量大小: 10
            # 三色氛围灯灯光效果
            0x01,  # 三色氛围灯灯光效果: 1
            # 三色氛围灯渐变模式
            0x01,  # 三色氛围灯渐变模式: 1
            # 三色氛围灯常亮RGB色R值
            0xFF,  # 三色氛围灯常亮RGB色R值: 255
            # 三色氛围灯常亮RGB色G值
            0x00,  # 三色氛围灯常亮RGB色G值: 0
            # 三色氛围灯常亮RGB色B值
            0x00,  # 三色氛围灯常亮RGB色B值: 0
            # 三色氛围灯呼吸RGB色R值
            0x00,  # 三色氛围灯呼吸RGB色R值: 0
            # 三色氛围灯呼吸RGB色G值
            0xFF,  # 三色氛围灯呼吸RGB色G值: 255
            # 三色氛围灯呼吸RGB色B值
            0x00,  # 三色氛围灯呼吸RGB色B值: 0
            # 三色氛围灯跑马灯RGB色R值
            0x00,  # 三色氛围灯跑马灯RGB色R值: 0
            # 三色氛围灯跑马灯RGB色G值
            0x00,  # 三色氛围灯跑马灯RGB色G值: 0
            # 三色氛围灯跑马灯RGB色B值
            0xFF,  # 三色氛围灯跑马灯RGB色B值: 255
            # 智能开关集1
            # Bit7: EBS开关 (1=开)
            # Bit6: 鞍座感应锁 (1=开)
            # Bit5: ABS紧急刹车警示 (1=开)
            # Bit4: 保留 (0)
            # Bit3: 位置灯常亮 (1=开)
            # Bit2: 紧急刹车告警 (1=开)
            # Bit1: 中箱锁 (1=开)
            # Bit0: 尾箱锁 (1=开)
            0b11101111,  # 0xEF
            # 智能开关集2
            # Bit7: 防盗器设防状态 (0x0b=声防盗设防)
            # Bit6: 防盗器灵敏度 (0x0b=振动灵敏度高)
            # Bit5: 迎宾指示灯 (1=开)
            # Bit4: 车辆双闪开关 (1=生效)
            # Bit3: 紧急模式 (1=开)
            # Bit2: 低电量续航模式 (1=开)
            # Bit1: 保留
            # Bit0: 保留
            0b11111100,  # 0xFC
            # 倒车开关
            0x01,  # 倒车开关: 开
            # 倒车速度
            0x0A,  # 倒车速度: 10
            # 防盗器设防状态
            0x0b,  # 防盗器设防状态: 声防盗设防
            # 防盗器灵敏度
            0x0b,  # 防盗器灵敏度: 振动灵敏度高
            # 防盗器工作音量
            0x0A,  # 防盗器工作音量: 10
            # 自动转向回位设置
            0x01,  # 自动转向回位设置: 1
            # EBS强度设置
            0x02,  # EBS强度设置: 标准默认
            # 低速挡扭矩
            0x64,  # 低速挡扭矩: 100
            # 中速挡扭矩
            0x7D,  # 中速挡扭矩: 125
            # 高速挡扭矩
            0x96,  # 高速挡扭矩: 150
            # 智能开关集3
            # Bit7: BSD功能 (1=开)
            # Bit6: LCW功能 (1=开)
            # Bit5: RCW功能 (1=开)
            # Bit4: 倒车辅助(雷达) (1=开)
            # Bit3: GPS开关 (1=开)
            # Bit2: 侧支架开关 (1=开)
            # Bit1: TCS开关 (1=开)
            # Bit0: 丢失模式 (1=开)
            0b11111111,  # 0xFF
            # 智能开关集4
            # Bit7: 充电指示灯 (1=开)
            # Bit6: 低电量续航模式 (1=开)
            # Bit5: 迎宾灯 (1=开)
            # Bit4: 雷达灵敏度 (0x02=高灵敏度)
            # Bit3: 陡坡缓降 (1=开)
            # Bit2: 防溜坡 (1=开)
            # Bit1: 保留
            # Bit0: 保留
            0b11111100,  # 0xFC
            # ECO档速度设置
            0x32,  # ECO档速度设置: 50
            # 起步强度设置
            0x02,  # 起步强度设置: 2
            # 蓝牙感应解锁灵敏度 (2字节)
            0x00, 0x64,  # 蓝牙感应解锁灵敏度: 100
            # SPORT档速度设置
            0x50,  # SPORT档速度设置: 80
            # EBS强度
            0x02  # EBS强度: 标准默认
        ])
        
        return response_data

# 创建全局模拟器实例
response_simulator = ResponseSimulator()
