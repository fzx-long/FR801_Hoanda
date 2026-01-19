#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
串口测试助手主程序入口
"""

import sys
import os

# 添加当前目录到系统路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from ui.main_window import main

if __name__ == "__main__":
    main()
