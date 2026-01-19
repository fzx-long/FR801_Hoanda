#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
简单测试脚本
"""

import sys

print("Python版本:", sys.version)
print("测试输出成功!")

# 尝试导入基本模块
try:
    import json
    print("json模块导入成功")
except Exception as e:
    print("导入json模块失败:", e)
