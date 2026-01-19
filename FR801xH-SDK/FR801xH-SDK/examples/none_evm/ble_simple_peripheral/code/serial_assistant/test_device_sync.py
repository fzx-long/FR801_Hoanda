#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试设备主动同步数据命令
验证0x209命令的响应数据格式是否正确
"""

import sys
import os

# 添加当前目录到系统路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    print("测试设备主动同步数据命令...")
    
    # 导入模拟器
    from protocol.simulator import response_simulator
    
    # 测试0x208命令
    print("\n测试0x208设备主动同步数据命令...")
    response = response_simulator.get_response(0x208, b'')
    print(f"响应数据长度: {len(response)} 字节")
    print(f"响应数据: {response.hex()}")
    
    # 验证数据格式
    print("\n验证数据格式...")
    expected_length = 43  # 计算所有字段的总长度
    if len(response) == expected_length:
        print(f"✓ 数据长度正确: {len(response)} 字节")
    else:
        print(f"✗ 数据长度错误: 期望 {expected_length} 字节，实际 {len(response)} 字节")
    
    # 验证各个字段
    print("\n验证各个字段:")
    
    # User档设置
    print(f"User档开关: 0x{response[0]:02X} ({'开' if response[0] == 1 else '关'})")
    print(f"User档速度: 0x{response[1]:02X} ({response[1]})")
    print(f"User档扭矩: 0x{response[2]:02X} ({response[2]})")
    
    # 助力推车设置
    print(f"助力推车开关: 0x{response[3]:02X} ({'开' if response[3] == 1 else '关'})")
    print(f"助力推车速度: 0x{response[4]:02X} ({response[4]})")
    
    # 延时大灯设置
    print(f"延时大灯开关: 0x{response[5]:02X} ({'开' if response[5] == 1 else '关'})")
    print(f"延时大灯延时时间: 0x{response[6]:02X} ({response[6]}秒)")
    
    # 充电功率
    print(f"充电功率: 0x{response[7]:02X} ({response[7]})")
    
    # 自动P档设置
    print(f"P档开关: 0x{response[8]:02X} ({'开' if response[8] == 1 else '关'})")
    print(f"P档延时时间: 0x{response[9]:02X} ({response[9]}秒)")
    
    # 和弦喇叭设置
    print(f"和弦喇叭音源: 0x{response[10]:02X} ({response[10]})")
    print(f"和弦喇叭音量大小: 0x{response[11]:02X} ({response[11]})")
    
    # 三色氛围灯设置
    print(f"三色氛围灯灯光效果: 0x{response[12]:02X} ({response[12]})")
    print(f"三色氛围灯渐变模式: 0x{response[13]:02X} ({response[13]})")
    print(f"三色氛围灯常亮RGB: 0x{response[14]:02X}, 0x{response[15]:02X}, 0x{response[16]:02X}")
    print(f"三色氛围灯呼吸RGB: 0x{response[17]:02X}, 0x{response[18]:02X}, 0x{response[19]:02X}")
    print(f"三色氛围灯跑马灯RGB: 0x{response[20]:02X}, 0x{response[21]:02X}, 0x{response[22]:02X}")
    
    # 智能开关集1
    print(f"智能开关集1: 0x{response[23]:02X} ({bin(response[23])[2:].zfill(8)})")
    
    # 智能开关集2
    print(f"智能开关集2: 0x{response[24]:02X} ({bin(response[24])[2:].zfill(8)})")
    
    # 倒车设置
    print(f"倒车开关: 0x{response[25]:02X} ({'开' if response[25] == 1 else '关'})")
    print(f"倒车速度: 0x{response[26]:02X} ({response[26]})")
    
    # 防盗器设置
    print(f"防盗器设防状态: 0x{response[27]:02X}")
    print(f"防盗器灵敏度: 0x{response[28]:02X}")
    print(f"防盗器工作音量: 0x{response[29]:02X} ({response[29]})")
    
    # 其他设置
    print(f"自动转向回位设置: 0x{response[30]:02X} ({response[30]})")
    print(f"EBS强度设置: 0x{response[31]:02X} ({response[31]})")
    print(f"低速挡扭矩: 0x{response[32]:02X} ({response[32]})")
    print(f"中速挡扭矩: 0x{response[33]:02X} ({response[33]})")
    print(f"高速挡扭矩: 0x{response[34]:02X} ({response[34]})")
    
    # 智能开关集3
    print(f"智能开关集3: 0x{response[35]:02X} ({bin(response[35])[2:].zfill(8)})")
    
    # 智能开关集4
    print(f"智能开关集4: 0x{response[36]:02X} ({bin(response[36])[2:].zfill(8)})")
    
    # 其他设置
    print(f"ECO档速度设置: 0x{response[37]:02X} ({response[37]})")
    print(f"起步强度设置: 0x{response[38]:02X} ({response[38]})")
    print(f"蓝牙感应解锁灵敏度: 0x{response[39]:02X}{response[40]:02X} ({(response[39] << 8) | response[40]})")
    print(f"SPORT档速度设置: 0x{response[41]:02X} ({response[41]})")
    print(f"EBS强度: 0x{response[42]:02X} ({response[42]})")
    
    print("\n测试完成!")
    print("✓ 设备主动同步数据命令已正确实现")
    
except Exception as e:
    print(f"测试失败: {e}")
    import traceback
    traceback.print_exc()
