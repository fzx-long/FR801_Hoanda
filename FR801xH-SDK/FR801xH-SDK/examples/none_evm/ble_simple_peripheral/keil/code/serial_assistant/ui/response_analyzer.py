#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
响应分析器
用于解析和显示接收到的数据
"""

from PyQt5.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QGroupBox, QLabel, 
    QTextEdit, QPushButton, QComboBox, QTabWidget, QTableWidget,
    QTableWidgetItem
)
from PyQt5.QtCore import Qt

class ResponseAnalyzer(QDialog):
    """
    响应分析器对话框
    """
    def __init__(self, parent=None, data=None, protocol='uart'):
        super().__init__(parent)
        self.setWindowTitle("响应分析器")
        self.setGeometry(200, 200, 700, 500)
        
        self.data = data
        self.protocol = protocol
        self.parsed_data = None
        
        self.init_ui()
    
    def init_ui(self):
        """
        初始化界面
        """
        main_layout = QVBoxLayout(self)
        
        # 顶部信息
        info_layout = QHBoxLayout()
        self.protocol_label = QLabel(f"协议: {self.protocol.upper()}")
        self.length_label = QLabel(f"长度: {len(self.data)} 字节")
        
        info_layout.addWidget(self.protocol_label)
        info_layout.addWidget(self.length_label)
        info_layout.addStretch()
        
        main_layout.addLayout(info_layout)
        
        # 标签页
        self.tab_widget = QTabWidget()
        
        # 原始数据标签
        raw_tab = QWidget()
        raw_layout = QVBoxLayout(raw_tab)
        
        raw_group = QGroupBox("原始数据")
        raw_group_layout = QVBoxLayout()
        
        self.raw_hex_text = QTextEdit()
        self.raw_hex_text.setReadOnly(True)
        raw_group_layout.addWidget(self.raw_hex_text)
        
        raw_group.setLayout(raw_group_layout)
        raw_layout.addWidget(raw_group)
        
        self.tab_widget.addTab(raw_tab, "原始数据")
        
        # 解析结果标签
        parse_tab = QWidget()
        parse_layout = QVBoxLayout(parse_tab)
        
        parse_group = QGroupBox("解析结果")
        parse_group_layout = QVBoxLayout()
        
        self.parse_text = QTextEdit()
        self.parse_text.setReadOnly(True)
        parse_group_layout.addWidget(self.parse_text)
        
        parse_group.setLayout(parse_group_layout)
        parse_layout.addWidget(parse_group)
        
        self.tab_widget.addTab(parse_tab, "解析结果")
        
        # 数据分析标签
        analysis_tab = QWidget()
        analysis_layout = QVBoxLayout(analysis_tab)
        
        analysis_group = QGroupBox("数据分析")
        analysis_group_layout = QVBoxLayout()
        
        self.analysis_table = QTableWidget()
        self.analysis_table.setColumnCount(2)
        self.analysis_table.setHorizontalHeaderLabels(["字段", "值"])
        self.analysis_table.horizontalHeader().setStretchLastSection(True)
        analysis_group_layout.addWidget(self.analysis_table)
        
        analysis_group.setLayout(analysis_group_layout)
        analysis_layout.addWidget(analysis_group)
        
        self.tab_widget.addTab(analysis_tab, "数据分析")
        
        main_layout.addWidget(self.tab_widget)
        
        # 按钮区域
        btn_layout = QHBoxLayout()
        save_btn = QPushButton("保存")
        close_btn = QPushButton("关闭")
        
        save_btn.clicked.connect(self.save_response)
        close_btn.clicked.connect(self.reject)
        
        btn_layout.addStretch()
        btn_layout.addWidget(save_btn)
        btn_layout.addWidget(close_btn)
        
        main_layout.addLayout(btn_layout)
        
        # 分析数据
        if self.data:
            self.analyze_data()
    
    def analyze_data(self):
        """
        分析数据
        """
        # 显示原始数据（十六进制）
        hex_str = ' '.join(f'{b:02X}' for b in self.data)
        self.raw_hex_text.setText(hex_str)
        
        # 尝试解析数据
        self.parse_data()
        
        # 填充分析表格
        self.fill_analysis_table()
    
    def parse_data(self):
        """
        解析数据
        """
        try:
            if self.protocol == 'uart':
                # UART协议解析
                from protocol.uart_protocol import SocMcu_Frame_Parse
                parsed = SocMcu_Frame_Parse(self.data)
                
                if parsed:
                    self.parsed_data = parsed
                    parse_info = f"解析成功！\n"
                    parse_info += f"同步字: 0x{parsed['sync']:02X}\n"
                    parse_info += f"Feature: 0x{parsed['feature']:04X}\n"
                    parse_info += f"命令: {parsed['cmd_name']}\n"
                    parse_info += f"命令ID: 0x{parsed['cmd_id']:02X}\n"
                    if parsed['data']:
                        data_hex = ' '.join(f'{b:02X}' for b in parsed['data'])
                        parse_info += f"数据: {data_hex}\n"
                        try:
                            parse_info += f"ASCII: {parsed['data'].decode('ascii', errors='replace')}\n"
                        except:
                            pass
                else:
                    parse_info = "解析失败：不是有效的UART协议帧"
            else:
                # BLE协议解析
                from protocol.ble_protocol import parse_ble_frame
                parsed = parse_ble_frame(self.data)
                
                if parsed:
                    self.parsed_data = parsed
                    parse_info = f"解析成功！\n"
                    parse_info += f"头部: 0x{parsed['header']:04X}\n"
                    parse_info += f"长度: {parsed['length']}\n"
                    parse_info += f"加密类型: {parsed['crypto']}\n"
                    parse_info += f"流水号: {parsed['seq_num']}\n"
                    parse_info += f"命令: 0x{parsed['cmd']:04X}\n"
                    if parsed['payload']:
                        payload_hex = ' '.join(f'{b:02X}' for b in parsed['payload'])
                        parse_info += f"负载: {payload_hex}\n"
                        try:
                            parse_info += f"ASCII: {parsed['payload'].decode('utf-8', errors='replace')}\n"
                        except:
                            pass
                else:
                    parse_info = "解析失败：不是有效的BLE协议帧"
            
            self.parse_text.setText(parse_info)
        except Exception as e:
            self.parse_text.setText(f"解析错误: {e}")
    
    def fill_analysis_table(self):
        """
        填充分析表格
        """
        self.analysis_table.setRowCount(0)
        
        # 基本信息
        self.add_table_row("总长度", f"{len(self.data)} 字节")
        self.add_table_row("协议", self.protocol.upper())
        
        # 十六进制表示
        hex_str = ' '.join(f'{b:02X}' for b in self.data)
        self.add_table_row("十六进制", hex_str)
        
        # ASCII表示
        try:
            ascii_str = self.data.decode('ascii', errors='replace')
            self.add_table_row("ASCII", ascii_str)
        except:
            pass
        
        # 解析后的数据
        if self.parsed_data:
            self.add_table_row("\n解析结果", "")
            
            if self.protocol == 'uart':
                self.add_table_row("同步字", f"0x{self.parsed_data['sync']:02X}")
                self.add_table_row("Feature", f"0x{self.parsed_data['feature']:04X}")
                self.add_table_row("命令", self.parsed_data['cmd_name'])
                self.add_table_row("命令ID", f"0x{self.parsed_data['cmd_id']:02X}")
                if self.parsed_data['data']:
                    data_hex = ' '.join(f'{b:02X}' for b in self.parsed_data['data'])
                    self.add_table_row("数据", data_hex)
            else:
                self.add_table_row("头部", f"0x{self.parsed_data['header']:04X}")
                self.add_table_row("长度", str(self.parsed_data['length']))
                self.add_table_row("加密类型", str(self.parsed_data['crypto']))
                self.add_table_row("流水号", str(self.parsed_data['seq_num']))
                self.add_table_row("命令", f"0x{self.parsed_data['cmd']:04X}")
                if self.parsed_data['payload']:
                    payload_hex = ' '.join(f'{b:02X}' for b in self.parsed_data['payload'])
                    self.add_table_row("负载", payload_hex)
    
    def add_table_row(self, key, value):
        """
        添加表格行
        """
        row = self.analysis_table.rowCount()
        self.analysis_table.insertRow(row)
        
        key_item = QTableWidgetItem(key)
        key_item.setFlags(Qt.ItemIsEnabled)
        value_item = QTableWidgetItem(value)
        value_item.setFlags(Qt.ItemIsEnabled)
        
        self.analysis_table.setItem(row, 0, key_item)
        self.analysis_table.setItem(row, 1, value_item)
    
    def save_response(self):
        """
        保存响应数据
        """
        from PyQt5.QtWidgets import QFileDialog
        
        file_path, _ = QFileDialog.getSaveFileName(
            self, "保存响应数据", "response.bin", "Binary Files (*.bin);;Text Files (*.txt)"
        )
        
        if file_path:
            try:
                if file_path.endswith('.txt'):
                    # 保存为文本文件
                    with open(file_path, 'w', encoding='utf-8') as f:
                        f.write(f"协议: {self.protocol}\n")
                        f.write(f"长度: {len(self.data)} 字节\n")
                        f.write("\n原始数据 (十六进制):\n")
                        f.write(' '.join(f'{b:02X}' for b in self.data))
                        f.write("\n\n解析结果:\n")
                        f.write(self.parse_text.toPlainText())
                else:
                    # 保存为二进制文件
                    with open(file_path, 'wb') as f:
                        f.write(self.data)
                
                from PyQt5.QtWidgets import QMessageBox
                QMessageBox.information(self, "成功", "响应数据已保存")
            except Exception as e:
                from PyQt5.QtWidgets import QMessageBox
                QMessageBox.warning(self, "错误", f"保存失败: {e}")
