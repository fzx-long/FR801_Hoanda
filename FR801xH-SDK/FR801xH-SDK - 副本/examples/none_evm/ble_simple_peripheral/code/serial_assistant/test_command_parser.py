#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys

# 添加当前目录到路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from protocol.command_parser import command_parser

print("Testing command parser...")
print(f"Command parser object: {command_parser}")

# 检查命令分类
categories = command_parser.get_all_categories()
print(f"\nCategories found: {len(categories)}")
for cat in categories:
    print(f"- {cat['name']} ({cat['key']})")

# 检查命令数量
all_commands = command_parser.get_all_commands()
print(f"\nTotal commands: {len(all_commands)}")

# 检查是否能获取命令
if all_commands:
    first_cmd = all_commands[0]
    print(f"First command: {first_cmd['name']} (0x{first_cmd['id']:02X})")

print("\nTest completed.")
