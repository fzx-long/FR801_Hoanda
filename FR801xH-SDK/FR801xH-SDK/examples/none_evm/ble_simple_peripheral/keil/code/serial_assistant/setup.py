#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
打包配置文件
用于PyInstaller打包应用程序
"""

import os
import sys
from PyInstaller.utils.hooks import collect_data_files

# 获取当前目录
current_dir = os.path.dirname(os.path.abspath(__file__))

# 打包配置
name = "串口测试助手"
version = "1.0.0"
author = "五羊本田"
description = "基于通用蓝牙协议的串口测试助手"

# 数据文件
datas = [
    (os.path.join(current_dir, 'resources'), 'resources'),
    (os.path.join(current_dir, 'data'), 'data'),
]

# 隐藏导入
hiddenimports = [
    'PyQt5.QtWidgets',
    'PyQt5.QtCore',
    'PyQt5.QtGui',
    'serial',
    'serial.tools.list_ports',
]

# 打包命令
if __name__ == '__main__':
    import PyInstaller.__main__
    
    # 构建命令参数
    args = [
        'main.py',
        '--name', name,
        '--onefile',
        '--windowed',
        '--add-data', f"{os.path.join(current_dir, 'resources')};resources",
        '--add-data', f"{os.path.join(current_dir, 'data')};data",
        '--hidden-import', 'PyQt5.QtWidgets',
        '--hidden-import', 'PyQt5.QtCore',
        '--hidden-import', 'PyQt5.QtGui',
        '--hidden-import', 'serial',
        '--hidden-import', 'serial.tools.list_ports',
    ]
    
    # 执行打包
    PyInstaller.__main__.run(args)
