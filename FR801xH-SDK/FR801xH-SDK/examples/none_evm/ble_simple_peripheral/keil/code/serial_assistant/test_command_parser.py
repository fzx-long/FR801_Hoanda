#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试命令解析器
用于检查208命令是否正确加载
"""

import sys
import os

# 添加当前目录到系统路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    print("测试命令解析器...")
    
    # 导入命令解析器
    from protocol.command_parser import command_parser
    
    # 测试加载命令
    current_dir = os.path.dirname(os.path.abspath(__file__))
    commands_file = os.path.join(current_dir, 'data', 'commands.json')
    
    print(f"命令文件路径: {commands_file}")
    print(f"文件是否存在: {os.path.exists(commands_file)}")
    
    # 重新加载命令
    if command_parser.load_commands(commands_file):
        print("命令加载成功")
    else:
        print("命令加载失败")
    
    # 测试获取所有分类
    print("\n测试获取所有分类...")
    categories = command_parser.get_all_categories()
    print(f"获取到 {len(categories)} 个分类:")
    for category in categories:
        print(f"- {category['name']} ({category['key']})")
    
    # 测试获取intelligent分类的命令
    print("\n测试获取intelligent分类的命令...")
    intelligent_commands = command_parser.get_commands_by_category('intelligent')
    print(f"intelligent分类有 {len(intelligent_commands)} 个命令:")
    for cmd in intelligent_commands:
        print(f"- {cmd['name']} ({cmd['hex']}) ID: {cmd['id']}")
    
    # 测试获取208命令
    print("\n测试获取208命令...")
    cmd_208 = command_parser.get_command_by_id(520)  # 0x208 = 520
    if cmd_208:
        print(f"找到208命令: {cmd_208['name']} ({cmd_208['hex']})")
        print(f"分类: {cmd_208['category_name']}")
        print(f"描述: {cmd_208['description']}")
    else:
        print("未找到208命令")
    
    # 测试获取209命令
    print("\n测试获取209命令...")
    cmd_209 = command_parser.get_command_by_id(521)  # 0x209 = 521
    if cmd_209:
        print(f"找到209命令: {cmd_209['name']} ({cmd_209['hex']})")
        print(f"分类: {cmd_209['category_name']}")
        print(f"描述: {cmd_209['description']}")
    else:
        print("未找到209命令")
    
    print("\n测试完成!")
    
except Exception as e:
    print(f"测试失败: {e}")
    import traceback
    traceback.print_exc()
