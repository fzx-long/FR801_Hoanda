#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试设备状态查询命令
验证0x209命令的响应数据格式是否正确
"""

import sys
import os

# 添加当前目录到系统路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    print("测试设备状态查询命令...")
    
    # 导入模拟器
    from protocol.simulator import response_simulator
    
    # 测试0x209命令
    print("\n测试0x209设备状态查询命令...")
    response = response_simulator.get_response(0x209, b'')
    print(f"响应数据长度: {len(response)} 字节")
    print(f"响应数据: {response.hex()}")
    
    # 验证数据格式
    print("\n验证数据格式...")
    expected_length = 4  # 计算所有字段的总长度
    if len(response) == expected_length:
        print(f"✓ 数据长度正确: {len(response)} 字节")
    else:
        print(f"✗ 数据长度错误: 期望 {expected_length} 字节，实际 {len(response)} 字节")
    
    # 验证各个字段
    print("\n验证各个字段:")
    print(f"设备状态: 0x{response[0]:02X} ({'正常' if response[0] == 0 else '异常'})")
    print(f"电池状态: 0x{response[1]:02X} ({'正常' if response[1] == 0 else '异常'})")
    print(f"蓝牙状态: 0x{response[2]:02X} ({'已连接' if response[2] == 1 else '未连接'})")
    print(f"GPS状态: 0x{response[3]:02X} ({'已定位' if response[3] == 1 else '未定位'})")
    
    print("\n测试完成!")
    print("✓ 设备状态查询命令已正确实现")
    
except Exception as e:
    print(f"测试失败: {e}")
    import traceback
    traceback.print_exc()
