#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys

# 添加当前目录到路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    import main
    print("Main module imported successfully")
except Exception as e:
    print(f"Error importing main module: {e}")
    import traceback
    traceback.print_exc()
