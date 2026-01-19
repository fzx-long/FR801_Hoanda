#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UART协议处理器
基于usart_device.c和usart_cmd.h的协议规范实现
"""

import struct

# 协议常量定义
SOC_MCU_FRAME_HEAD = 0xFE
SOC_MCU_FRAME_END0 = 0x0A
SOC_MCU_FRAME_END1 = 0x0D

SOC_MCU_SYNC_SOC_TO_MCU = 0xAB
SOC_MCU_SYNC_MCU_TO_SOC = 0xBA

# 通道定义
SOC_MCU_FEATURE_CMD = 0xFF01        # 命令
SOC_MCU_FEATURE_RESP = 0xFF02       # 命令应答
SOC_MCU_FEATURE_SETTINGS = 0xFF03   # 设置
SOC_MCU_FEATURE_RESP_SETTINGS = 0xFF04  # 设置应答

SOC_MCU_USE_CRC16 = 0
SOC_MCU_CRC_LEN = 2 if SOC_MCU_USE_CRC16 else 1
SOC_MCU_FRAME_MAX_LEN = 256

# 结果码定义
RESULT_SUCCESS = 0x00  # 成功
RESULT_FAILURE = 0x01  # 失败
RESULT_PARAM_ERROR = 0x02  # 参数错误
RESULT_NOT_SUPPORTED = 0x03  # 不支持
RESULT_BUSY = 0x04  # 忙
RESULT_STATUS_ERROR = 0x05  # 状态错误

# 命令定义
COMMANDS = {
    0x01: "蓝牙连接命令",
    0x16: "蓝牙恢复出厂设置命令",
    0x17: "取消蓝牙配对命令",
    0x20: "添加NFC钥匙命令",
    0x21: "删除NFC钥匙命令",
    0x22: "查找NFC钥匙命令",
    0x23: "NFC解锁命令",
    0x24: "NFC锁车命令",
    0x25: "NFC开关命令",
    0x30: "油车解防命令",
    0x31: "油车防盗命令",
    0x32: "油车寻车状态查询命令",
    0x33: "油车尾箱解锁命令",
    0x34: "油车尾箱锁车命令",
    0x35: "座桶解锁命令",
    0x36: "座桶锁车命令",
    0x37: "静音设置命令",
    0x38: "中箱锁解锁命令",
    0x39: "中箱锁锁车命令",
    0x40: "紧急模式打开命令",
    0x41: "紧急模式关闭命令",
    0x42: "读取蓝牙MAC地址命令",
    0x43: "单控开锁命令（尾箱锁）",
    0x44: "助力推车模式开启命令",
    0x45: "助力推车模式关闭命令",
    0x46: "默认助力推车模式命令",
    0x47: "延时大灯开启命令",
    0x48: "延时大灯关闭命令",
    0x49: "默认延时大灯命令",
    0x50: "延时大灯时间设置命令",
    0x51: "充电功率设置命令",
    0x52: "自动P档开启命令",
    0x53: "自动P档关闭命令",
    0x54: "自动P档默认命令",
    0x55: "自动P档时间设置命令",
    0x56: "和弦喇叭开启命令",
    0x57: "和弦喇叭关闭命令",
    0x58: "和弦喇叭默认命令",
    0x59: "和弦喇叭音源设置命令",
    0x60: "和弦喇叭音量设置命令",
    0x61: "三射灯氛围灯开启命令",
    0x62: "三射灯氛围灯关闭命令",
    0x63: "三射灯氛围灯默认命令",
    0x64: "三射灯氛围灯模式设置命令",
    0x65: "辅助倒车档位开启命令",
    0x66: "辅助倒车档位关闭命令",
    0x67: "辅助倒车档位默认命令",
    0x75: "自动转向回位开启命令",
    0x76: "自动转向回位关闭命令",
    0x77: "自动转向回位默认命令",
    0x78: "EBS开启命令",
    0x79: "低速档位开启命令",
    0x80: "低速档位关闭命令",
    0x81: "低速档位默认命令",
    0x82: "低速档位速度设置命令",
    0x83: "中速档位开启命令",
    0x84: "中速档位关闭命令",
    0x85: "中速档位默认命令",
    0x86: "中速档位速度设置命令",
    0x87: "高速档位开启命令",
    0x88: "高速档位关闭命令",
    0x89: "高速档位默认命令",
    0x90: "高速档位速度设置命令",
    0x91: "丢失模式开启命令",
    0x92: "丢失模式关闭命令",
    0x93: "TCS开启命令",
    0x94: "TCS关闭命令",
    0x95: "侧支架开启命令",
    0x96: "侧支架关闭命令",
    0x97: "电池模式设置命令",
    0x98: "电池容量设置命令",
    0x99: "KFA、2KGA、2MQA数据同步命令",
    0x100: "HDC开启命令",
    0x101: "HDC关闭命令",
    0x102: "HHC开启命令",
    0x103: "HHC关闭命令",
    0x104: "起步强度设置命令",
    0x105: "SPORT模式开启命令",
    0x106: "SPORT模式关闭命令",
    0x107: "SPORT模式默认命令",
    0x108: "SPORT模式类型设置命令",
    0x109: "ECO模式开启命令",
    0x110: "ECO模式关闭命令",
    0x111: "ECO模式默认命令",
    0x112: "ECO模式类型设置命令",
    0x113: "雷达开启命令",
    0x114: "雷达关闭命令",
    0x115: "雷达默认命令",
    0x116: "雷达灵敏度设置命令",
    0x117: "胎压监测获取命令",
    0x118: "读取蓝牙RSSI值命令",
    0x119: "蓝牙无感解锁灵敏度命令",
    0x120: "蓝牙无感解锁开命令",
    0x121: "蓝牙无感解锁关命令",
    0x11A: "胎压传感器状态获取命令",
    0x11B: "胎压传感器配对命令",
    0xA0: "防盗设置灵敏度读取命令",
    0xA1: "防盗设置灵敏度设置命令",
    0x200: "充电显示器开命令",
    0x201: "充电显示器关命令",
    0x202: "动力回收开命令(EBS)",
    0x203: "动力回收关命令(EBS)",
    0x208: "设备主动同步数据命令",
    0x209: "设备状态查询命令"
}

def bin_complement_check_sum(data):
    """
    二进制补码校验和
    对应C代码中的BinComplementCheckSum函数
    """
    sum_val = 0
    for byte in data:
        sum_val += byte
    # 计算补码加1
    return (~sum_val + 1) & 0xFF  # 保持8位

def put_u16_be(buf, offset, value):
    """
    大端序写入16位值
    """
    buf[offset] = (value >> 8) & 0xFF
    buf[offset + 1] = value & 0xFF

def SocMcu_Frame_Build(sync, feature, cmd_id, data=None):
    """
    构建SOC↔MCU UART二进制帧
    
    参数：
        sync: 同步字（0xAB=SOC→MCU / 0xBA=MCU→SOC）
        feature: 通道字段（SOC_MCU_FEATURE_*）
        cmd_id: 命令ID
        data: 数据段（可选）
    
    返回：
        构建好的帧字节列表
    """
    data_len = len(data) if data else 0
    
    # 计算总长度
    total_len = 1 + 1 + 2 + 2 + 2 + data_len + SOC_MCU_CRC_LEN + 2
    
    # 构建帧
    frame = bytearray(total_len)
    
    # 填充帧头
    frame[0] = SOC_MCU_FRAME_HEAD
    frame[1] = sync & 0xFF
    
    # 填充feature和cmd_id（大端序）
    put_u16_be(frame, 2, feature)
    put_u16_be(frame, 4, cmd_id)
    
    # 填充长度字段（直接使用data段的长度）
    len_field = data_len
    put_u16_be(frame, 6, len_field)
    
    # 填充数据
    if data_len > 0:
        frame[8:8+data_len] = data
    
    # 计算校验和
    crc_offset = 8 + data_len
    # 校验和覆盖范围：head(1B)+sync(1B)+feature(2B)+id(2B)+len(2B)+data(nB)
    checksum_data = frame[0:8+data_len]
    checksum = bin_complement_check_sum(checksum_data)
    frame[crc_offset] = checksum
    
    # 填充帧尾
    end_offset = crc_offset + SOC_MCU_CRC_LEN
    frame[end_offset] = SOC_MCU_FRAME_END0
    frame[end_offset + 1] = SOC_MCU_FRAME_END1
    
    return frame

def build_response_frame(cmd_id, seq, result, payload=None):
    """
    构建标准响应帧
    
    参数：
        cmd_id: 命令ID
        seq: 序列号
        result: 结果码（RESULT_*）
        payload: 附加数据（可选）
    
    返回：
        构建好的响应帧
    """
    # 构建响应数据：seq(1B) + result(1B) + payload(...)
    data_parts = [seq.to_bytes(1, 'big'), result.to_bytes(1, 'big')]
    if payload:
        data_parts.append(payload)
    response_data = b''.join(data_parts)
    
    # 构建完整帧
    return SocMcu_Frame_Build(
        SOC_MCU_SYNC_MCU_TO_SOC,  # 回包sync
        SOC_MCU_FEATURE_RESP,     # 命令应答通道
        cmd_id,                   # 命令ID
        response_data             # 响应数据
    )

def SocMcu_Frame_Parse(frame):
    """
    解析SOC↔MCU UART二进制帧
    
    参数：
        frame: 接收到的帧字节列表
    
    返回：
        解析结果字典，包含sync、feature、cmd_id、data、direction等字段
        解析失败返回None
    """
    try:
        # 检查最小长度
        if len(frame) < 1 + 1 + 2 + 2 + 2 + SOC_MCU_CRC_LEN + 2:
            return None
        
        # 检查帧头
        if frame[0] != SOC_MCU_FRAME_HEAD:
            return None
        
        # 解析基本字段
        sync = frame[1]
        feature = (frame[2] << 8) | frame[3]
        cmd_id = (frame[4] << 8) | frame[5]
        len_field = (frame[6] << 8) | frame[7]
        
        # 计算数据长度（直接使用len_field）
        data_len = len_field
        if data_len < 0:
            return None
        
        # 检查总长度
        expected_len = 1 + 1 + 2 + 2 + 2 + data_len + SOC_MCU_CRC_LEN + 2
        if len(frame) != expected_len:
            return None
        
        # 提取数据
        if data_len > 0:
            data = frame[8:8+data_len]
        else:
            data = b''
        
        # 验证校验和
        crc_offset = 8 + data_len
        crc_valid = True
        crc_value = 0
        
        # 计算校验和 - 包含head字段
        checksum_data = frame[0:8+data_len]
        expected_checksum = frame[crc_offset]
        actual_checksum = bin_complement_check_sum(checksum_data)
        crc_valid = (actual_checksum == expected_checksum)
        crc_value = expected_checksum
        
        # 检查帧尾
        end_offset = crc_offset + SOC_MCU_CRC_LEN
        if frame[end_offset] != SOC_MCU_FRAME_END0 or frame[end_offset + 1] != SOC_MCU_FRAME_END1:
            return None
        
        # 判断传输方向
        direction = "SOC→MCU" if sync == SOC_MCU_SYNC_SOC_TO_MCU else "MCU→SOC"
        direction_code = "SOC_TO_MCU" if sync == SOC_MCU_SYNC_SOC_TO_MCU else "MCU_TO_SOC"
        
        # 返回解析结果
        return {
            'sync': sync,
            'sync_hex': f"0x{sync:02X}",
            'direction': direction,
            'direction_code': direction_code,
            'feature': feature,
            'feature_hex': f"0x{feature:04X}",
            'cmd_id': cmd_id,
            'cmd_id_hex': f"0x{cmd_id:02X}",
            'data': data,
            'data_len': data_len,
            'data_hex': format_data_hex(data),
            'crc_valid': crc_valid,
            'crc_value': crc_value,
            'crc_hex': f"0x{crc_value:04X}",
            'cmd_name': COMMANDS.get(cmd_id, f"未知命令 0x{cmd_id:02X}"),
            'total_len': len(frame),
            'raw_frame': frame
        }
        
    except Exception:
        return None

def format_frame_hex(frame):
    """
    将帧格式化为十六进制字符串
    """
    return ' '.join(f'{b:02X}' for b in frame)

def format_data_hex(data):
    """
    将数据格式化为十六进制字符串
    """
    return ' '.join(f'{b:02X}' for b in data)

def format_data_ascii(data):
    """
    将数据格式化为ASCII字符串
    """
    try:
        return data.decode('ascii', errors='replace')
    except:
        return str(data)
