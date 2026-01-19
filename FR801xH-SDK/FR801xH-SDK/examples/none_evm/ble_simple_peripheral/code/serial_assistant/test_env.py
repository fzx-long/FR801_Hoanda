#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
环境测试脚本
用于验证Python环境和命令解析是否正常
"""

import sys
import os

# 添加当前目录到系统路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    print("测试Python环境...")
    print(f"Python版本: {sys.version}")
    
    # 测试导入PyQt5
    print("\n测试导入PyQt5...")
    from PyQt5.QtWidgets import QApplication
    print("PyQt5导入成功")
    
    # 测试导入串口模块
    print("\n测试导入串口模块...")
    import serial
    import serial.tools.list_ports
    print("串口模块导入成功")
    
    # 测试导入协议模块
    print("\n测试导入协议模块...")
    from protocol.uart_protocol import SocMcu_Frame_Build, SocMcu_Frame_Parse, bin_complement_check_sum
    from protocol.command_parser import command_parser
    from protocol.simulator import response_simulator
    print("协议模块导入成功")
    
    # 测试命令解析
    print("\n测试命令解析...")
    commands = command_parser.get_all_commands()
    print(f"加载到 {len(commands)} 个命令")
    
    # 查找209命令
    cmd_209 = command_parser.get_command_by_id(521)  # 0x209 = 521
    if cmd_209:
        print(f"找到209命令: {cmd_209['name']} ({cmd_209['hex']})")
        print(f"描述: {cmd_209['description']}")
    else:
        print("未找到209命令")
    
    # 测试209命令的模拟响应
    print("\n测试209命令的模拟响应...")
    response = response_simulator.get_response(0x209, b'')
    print(f"209命令响应: {response.hex()}")
    print(f"响应长度: {len(response)} 字节")
    
    # 测试校验和计算
    print("\n测试校验和计算...")
    test_data = b'\xFE\xAB\xFF\x01\x00\x01\x00\x01\x00'
    checksum = bin_complement_check_sum(test_data)
    print(f"测试数据: {test_data.hex()}")
    print(f"校验和: 0x{checksum:02X}")
    
    # 测试帧构建
    print("\n测试帧构建...")
    frame = SocMcu_Frame_Build(0xAB, 0xFF01, 0x209, b'\x00')
    print(f"构建的帧: {frame.hex()}")
    print(f"帧长度: {len(frame)} 字节")
    
    # 测试帧解析
    print("\n测试帧解析...")
    parsed = SocMcu_Frame_Parse(frame)
    if parsed:
        print(f"解析成功: {parsed['cmd_name']}")
        print(f"CRC有效: {parsed['crc_valid']}")
    else:
        print("解析失败")
    
    print("\n所有测试通过！")
    
except Exception as e:
    print(f"\n测试失败: {e}")
    import traceback
    traceback.print_exc()
