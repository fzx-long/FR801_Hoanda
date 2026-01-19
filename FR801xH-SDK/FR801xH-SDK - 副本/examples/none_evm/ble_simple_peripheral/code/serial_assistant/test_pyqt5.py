#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
from PyQt5.QtWidgets import QApplication, QMainWindow, QLabel

print("Testing PyQt5...")
print(f"Python version: {sys.version}")

# 测试PyQt5是否能正常导入和使用
try:
    app = QApplication(sys.argv)
    window = QMainWindow()
    window.setWindowTitle("PyQt5 Test")
    window.setGeometry(100, 100, 300, 200)
    
    label = QLabel("PyQt5 is working!")
    window.setCentralWidget(label)
    
    # 不显示窗口，只测试初始化
    print("PyQt5 initialized successfully")
    print("PyQt5 test completed successfully")
    sys.exit(0)
except Exception as e:
    print(f"Error testing PyQt5: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
