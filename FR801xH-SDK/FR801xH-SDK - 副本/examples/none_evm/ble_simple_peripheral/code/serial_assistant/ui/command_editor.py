#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
命令编辑器
提供可视化的命令参数编辑功能
"""

from PyQt5.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QGroupBox, QLabel, 
    QLineEdit, QPushButton, QComboBox, QSpinBox, QTextEdit,
    QFormLayout, QCheckBox, QGridLayout
)
from PyQt5.QtCore import Qt

class CommandEditor(QDialog):
    """
    命令编辑器对话框
    """
    def __init__(self, parent=None, cmd_data=None):
        super().__init__(parent)
        self.setWindowTitle("命令编辑器")
        self.setGeometry(200, 200, 600, 400)
        
        self.cmd_data = cmd_data
        self.result_data = b''
        
        self.init_ui()
    
    def init_ui(self):
        """
        初始化界面
        """
        main_layout = QVBoxLayout(self)
        
        # 命令基本信息
        info_group = QGroupBox("命令信息")
        info_layout = QFormLayout()
        
        self.cmd_name_edit = QLineEdit()
        self.cmd_name_edit.setReadOnly(True)
        self.cmd_id_edit = QLineEdit()
        self.cmd_id_edit.setReadOnly(True)
        self.cmd_desc_edit = QTextEdit()
        self.cmd_desc_edit.setReadOnly(True)
        self.cmd_desc_edit.setFixedHeight(80)
        
        info_layout.addRow("命令名称:", self.cmd_name_edit)
        info_layout.addRow("命令ID:", self.cmd_id_edit)
        info_layout.addRow("命令描述:", self.cmd_desc_edit)
        
        info_group.setLayout(info_layout)
        main_layout.addWidget(info_group)
        
        # 参数编辑
        param_group = QGroupBox("参数编辑")
        param_layout = QVBoxLayout()
        
        # 数据输入模式选择
        mode_layout = QHBoxLayout()
        self.hex_mode = QCheckBox("十六进制模式")
        self.hex_mode.setChecked(True)
        self.ascii_mode = QCheckBox("ASCII模式")
        
        mode_layout.addWidget(self.hex_mode)
        mode_layout.addWidget(self.ascii_mode)
        mode_layout.addStretch()
        
        param_layout.addLayout(mode_layout)
        
        # 数据输入区域
        self.data_input = QTextEdit()
        self.data_input.setPlaceholderText("输入数据（十六进制，空格分隔）")
        param_layout.addWidget(self.data_input)
        
        # 常用数据模板
        template_layout = QHBoxLayout()
        template_label = QLabel("常用模板:")
        self.template_combo = QComboBox()
        self.template_combo.addItems([
            "空数据",
            "00",
            "01",
            "FF",
            "00 01",
            "01 02 03 04"
        ])
        apply_template_btn = QPushButton("应用")
        apply_template_btn.clicked.connect(self.apply_template)
        
        template_layout.addWidget(template_label)
        template_layout.addWidget(self.template_combo)
        template_layout.addWidget(apply_template_btn)
        template_layout.addStretch()
        
        param_layout.addLayout(template_layout)
        
        param_group.setLayout(param_layout)
        main_layout.addWidget(param_group)
        
        # 预览区域
        preview_group = QGroupBox("数据预览")
        preview_layout = QVBoxLayout()
        
        self.preview_text = QTextEdit()
        self.preview_text.setReadOnly(True)
        preview_layout.addWidget(self.preview_text)
        
        preview_group.setLayout(preview_layout)
        main_layout.addWidget(preview_group)
        
        # 按钮区域
        btn_layout = QHBoxLayout()
        ok_btn = QPushButton("确定")
        cancel_btn = QPushButton("取消")
        
        ok_btn.clicked.connect(self.accept)
        cancel_btn.clicked.connect(self.reject)
        
        btn_layout.addStretch()
        btn_layout.addWidget(ok_btn)
        btn_layout.addWidget(cancel_btn)
        
        main_layout.addLayout(btn_layout)
        
        # 加载命令数据
        if self.cmd_data:
            self.load_cmd_data()
    
    def load_cmd_data(self):
        """
        加载命令数据
        """
        self.cmd_name_edit.setText(self.cmd_data.get('name', ''))
        self.cmd_id_edit.setText(self.cmd_data.get('hex', ''))
        self.cmd_desc_edit.setText(self.cmd_data.get('description', ''))
    
    def apply_template(self):
        """
        应用数据模板
        """
        template = self.template_combo.currentText()
        self.data_input.setText(template)
        self.update_preview()
    
    def update_preview(self):
        """
        更新数据预览
        """
        try:
            data_str = self.data_input.toPlainText().strip()
            if self.hex_mode.isChecked():
                # 十六进制模式
                if data_str:
                    data = bytes.fromhex(data_str.replace(' ', ''))
                    preview = f"十六进制: {data_str}\n"
                    preview += f"ASCII: {data.decode('ascii', errors='replace')}\n"
                    preview += f"长度: {len(data)} 字节"
                else:
                    preview = "空数据"
            else:
                # ASCII模式
                data = data_str.encode('ascii')
                hex_str = ' '.join(f'{b:02X}' for b in data)
                preview = f"ASCII: {data_str}\n"
                preview += f"十六进制: {hex_str}\n"
                preview += f"长度: {len(data)} 字节"
            
            self.preview_text.setText(preview)
        except Exception as e:
            self.preview_text.setText(f"错误: {e}")
    
    def get_data(self):
        """
        获取编辑后的数据
        
        返回：
            字节数据
        """
        try:
            data_str = self.data_input.toPlainText().strip()
            if not data_str:
                return b''
            
            if self.hex_mode.isChecked():
                # 十六进制模式
                return bytes.fromhex(data_str.replace(' ', ''))
            else:
                # ASCII模式
                return data_str.encode('ascii')
        except:
            return b''
    
    def accept(self):
        """
        确认按钮点击事件
        """
        self.result_data = self.get_data()
        super().accept()
    
    def get_result(self):
        """
        获取结果数据
        
        返回：
            字节数据
        """
        return self.result_data
