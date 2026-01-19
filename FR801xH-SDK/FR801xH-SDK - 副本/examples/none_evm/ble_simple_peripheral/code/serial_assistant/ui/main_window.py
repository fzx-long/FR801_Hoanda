#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
主窗口界面实现
"""

import sys
import os
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
    QGroupBox, QLabel, QComboBox, QPushButton, QTextEdit, 
    QSplitter, QTreeWidget, QTreeWidgetItem, QLineEdit,
    QTabWidget, QRadioButton, QButtonGroup, QSpinBox,
    QStatusBar, QFileDialog, QMessageBox, QCheckBox, QDialog
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
from .command_editor import CommandEditor
from .response_analyzer import ResponseAnalyzer
from PyQt5.QtGui import QFont, QIcon
import os

# 导入协议模块
current_dir = os.path.dirname(os.path.dirname(__file__))
sys.path.insert(0, current_dir)

from protocol.uart_protocol import SocMcu_Frame_Build, SocMcu_Frame_Parse, format_frame_hex
from protocol.ble_protocol import build_ble_frame, parse_ble_frame as parse_ble_frame_ble
from protocol.command_parser import command_parser
from protocol.simulator import response_simulator

import serial
import serial.tools.list_ports

class SerialThread(QThread):
    """
    串口通信线程
    """
    data_received = pyqtSignal(bytes)
    error_occurred = pyqtSignal(str)
    
    def __init__(self, port, baudrate):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.serial = None
        self.running = False
    
    def run(self):
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=0.1
            )
            self.running = True
            
            while self.running:
                try:
                    if self.serial.in_waiting > 0:
                        data = self.serial.read(self.serial.in_waiting)
                        self.data_received.emit(data)
                except Exception as e:
                    self.error_occurred.emit(f"读取错误: {e}")
                    break
        except Exception as e:
            self.error_occurred.emit(f"串口错误: {e}")
    
    def stop(self):
        self.running = False
        if self.serial and self.serial.is_open:
            self.serial.close()
    
    def send(self, data):
        if self.serial and self.serial.is_open:
            try:
                self.serial.write(data)
                return True
            except Exception as e:
                self.error_occurred.emit(f"发送错误: {e}")
                return False
        return False

class MainWindow(QMainWindow):
    """
    主窗口
    """
    def __init__(self):
        super().__init__()
        self.setWindowTitle("串口测试助手")
        self.setGeometry(100, 100, 1300, 800)
        
        # 设置应用程序样式
        self.setStyleSheet("""
            QMainWindow {
                background-color: #f8f9fa;
            }
            QGroupBox {
                background-color: white;
                border: 1px solid #ced4da;
                border-radius: 4px;
                margin-top: 10px;
                padding: 10px;
            }
            QPushButton {
                background-color: #007bff;
                color: white;
                border: none;
                padding: 8px 16px;
                border-radius: 6px;
                font-weight: 500;
                font-size: 10pt;
            }
            QPushButton:hover {
                background-color: #0056b3;
            }
            QPushButton:pressed {
                background-color: #004085;
            }
            QPushButton:disabled {
                background-color: #6c757d;
                color: #adb5bd;
            }
            QComboBox {
                padding: 6px 10px;
                border: 1px solid #ced4da;
                border-radius: 6px;
                background-color: white;
                font-size: 10pt;
                min-height: 24px;
            }
            QComboBox:hover {
                border: 1px solid #80bdff;
            }
            QComboBox::drop-down {
                border: none;
                width: 20px;
            }
            QComboBox::down-arrow {
                image: none;
                border-left: 5px solid transparent;
                border-right: 5px solid transparent;
                border-top: 5px solid #6c757d;
                margin-right: 5px;
            }
            QTextEdit {
                border: 1px solid #ced4da;
                border-radius: 6px;
                background-color: white;
                font-family: 'Consolas', 'Courier New', monospace;
                font-size: 10pt;
                padding: 8px;
                selection-background-color: #007bff;
                selection-color: white;
            }
            QTextEdit:focus {
                border: 2px solid #007bff;
            }
            QTreeWidget {
                border: 1px solid #ced4da;
                border-radius: 6px;
                background-color: white;
                font-size: 10pt;
                selection-background-color: #007bff;
                selection-color: white;
                outline: none;
            }
            QTreeWidget::item {
                height: 28px;
                padding: 4px;
            }
            QTreeWidget::item:hover {
                background-color: #e9ecef;
            }
            QTreeWidget::item:selected {
                background-color: #007bff;
                color: white;
            }
            QTreeWidget::branch {
                background-color: white;
            }
            QTreeWidget::branch:has-children:!has-siblings:closed,
            QTreeWidget::branch:closed:has-children:has-siblings {
                border-image: none;
                image: none;
            }
            QTreeWidget::branch:open:has-children:!has-siblings,
            QTreeWidget::branch:open:has-children:has-siblings {
                border-image: none;
                image: none;
            }
            QTreeWidget::branch:has-children:!has-siblings:closed,
            QTreeWidget::branch:closed:has-children:has-siblings {
                border-image: none;
                image: none;
            }
            QTreeWidget::branch:open:has-children:!has-siblings,
            QTreeWidget::branch:open:has-children:has-siblings {
                border-image: none;
                image: none;
            }
            QTreeWidget::indicator {
                width: 16px;
                height: 16px;
            }
            QTreeWidget::indicator:closed {
                image: none;
                border: none;
                width: 0;
                height: 0;
                border-left: 6px solid transparent;
                border-right: 6px solid transparent;
                border-bottom: 6px solid #ced4da;
            }
            QTreeWidget::indicator:open {
                image: none;
                border: none;
                width: 0;
                height: 0;
                border-left: 6px solid transparent;
                border-right: 6px solid transparent;
                border-top: 6px solid #ced4da;
            }
            QTreeWidget::indicator:closed:hover {
                border-bottom: 6px solid #007bff;
            }
            QTreeWidget::indicator:open:hover {
                border-top: 6px solid #007bff;
            }
            QTreeWidget::indicator:branch {
                background-color: white;
            }
            QLabel {
                color: #495057;
                font-size: 10pt;
                font-weight: 500;
            }
            QRadioButton {
                color: #495057;
                font-size: 10pt;
                spacing: 8px;
            }
            QRadioButton::indicator {
                width: 18px;
                height: 18px;
                border: 2px solid #ced4da;
                border-radius: 9px;
                background-color: white;
            }
            QRadioButton::indicator:checked {
                background-color: #007bff;
                border: 2px solid #007bff;
            }
            QCheckBox {
                color: #495057;
                font-size: 10pt;
                spacing: 8px;
            }
            QCheckBox::indicator {
                width: 18px;
                height: 18px;
                border: 2px solid #ced4da;
                border-radius: 3px;
                background-color: white;
            }
            QCheckBox::indicator:checked {
                background-color: #007bff;
                border: 2px solid #007bff;
            }
            QStatusBar {
                background-color: #343a40;
                color: white;
                border-top: 2px solid #007bff;
                font-size: 10pt;
            }
            QSplitter::handle {
                background-color: #dee2e6;
                width: 2px;
            }
            QSplitter::handle:hover {
                background-color: #007bff;
            }
        """)
        
        # 设置窗口图标
        icon_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'resources', 'icon.png')
        if os.path.exists(icon_path):
            self.setWindowIcon(QIcon(icon_path))
        
        # 初始化变量
        self.serial_thread = None
        self.current_protocol = 'uart'  # 'uart' 或 'ble'
        self.current_cmd_id = None
        self.seq_num = 0
        self.rssi_filter_enabled = False
        
        # 创建中心部件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # 主布局
        main_layout = QVBoxLayout(central_widget)
        main_layout.setSpacing(10)
        main_layout.setContentsMargins(15, 15, 15, 15)
        
        # 顶部配置区域
        config_group = QGroupBox("配置")
        config_layout = QHBoxLayout()
        config_layout.setSpacing(15)
        
        # 串口配置
        serial_group = QGroupBox("串口")
        serial_layout = QVBoxLayout()
        serial_layout.setSpacing(8)
        
        port_layout = QHBoxLayout()
        port_layout.addWidget(QLabel("端口:"))
        self.port_combo = QComboBox()
        self.refresh_ports()
        port_layout.addWidget(self.port_combo)
        refresh_port_btn = QPushButton("刷新")
        refresh_port_btn.clicked.connect(self.refresh_ports)
        port_layout.addWidget(refresh_port_btn)
        serial_layout.addLayout(port_layout)
        
        baud_layout = QHBoxLayout()
        baud_layout.addWidget(QLabel("波特率:"))
        self.baud_combo = QComboBox()
        self.baud_combo.addItems(["9600", "19200", "38400", "57600", "115200"])
        self.baud_combo.setCurrentText("115200")
        baud_layout.addWidget(self.baud_combo)
        serial_layout.addLayout(baud_layout)
        
        connect_layout = QHBoxLayout()
        self.connect_btn = QPushButton("连接")
        self.connect_btn.clicked.connect(self.toggle_connection)
        connect_layout.addWidget(self.connect_btn)
        serial_layout.addLayout(connect_layout)
        
        serial_group.setLayout(serial_layout)
        config_layout.addWidget(serial_group)
        
        # 协议选择
        protocol_group = QGroupBox("协议")
        protocol_layout = QVBoxLayout()
        protocol_layout.setSpacing(8)
        
        self.protocol_group = QButtonGroup()
        uart_radio = QRadioButton("UART协议")
        uart_radio.setChecked(True)
        uart_radio.toggled.connect(lambda: self.set_protocol('uart'))
        ble_radio = QRadioButton("BLE协议")
        ble_radio.toggled.connect(lambda: self.set_protocol('ble'))
        
        self.protocol_group.addButton(uart_radio)
        self.protocol_group.addButton(ble_radio)
        
        protocol_layout.addWidget(uart_radio)
        protocol_layout.addWidget(ble_radio)
        
        # 方向选择
        direction_group = QGroupBox("方向")
        direction_layout = QVBoxLayout()
        direction_layout.setSpacing(4)
        
        self.direction_group = QButtonGroup()
        soc_to_mcu_radio = QRadioButton("SOC → MCU")
        soc_to_mcu_radio.setChecked(True)
        mcu_to_soc_radio = QRadioButton("MCU → SOC")
        
        self.direction_group.addButton(soc_to_mcu_radio)
        self.direction_group.addButton(mcu_to_soc_radio)
        
        direction_layout.addWidget(soc_to_mcu_radio)
        direction_layout.addWidget(mcu_to_soc_radio)
        direction_group.setLayout(direction_layout)
        protocol_layout.addWidget(direction_group)
        
        # 模拟回复选项
        self.simulate_response = QCheckBox("模拟回复")
        self.simulate_response.setChecked(False)
        protocol_layout.addWidget(self.simulate_response)
        
        # RSSI过滤选项
        self.rssi_filter_btn = QPushButton("开启RSSI过滤")
        self.rssi_filter_btn.clicked.connect(self.toggle_rssi_filter)
        protocol_layout.addWidget(self.rssi_filter_btn)
        
        protocol_group.setLayout(protocol_layout)
        config_layout.addWidget(protocol_group)
        
        config_group.setLayout(config_layout)
        main_layout.addWidget(config_group)
        
        # 中间命令和数据区域
        middle_splitter = QSplitter(Qt.Horizontal)
        
        # 左侧命令导航
        cmd_nav_widget = QWidget()
        cmd_nav_layout = QVBoxLayout(cmd_nav_widget)
        cmd_nav_layout.setSpacing(10)
        cmd_nav_layout.setContentsMargins(5, 5, 5, 5)
        
        cmd_nav_group = QGroupBox("命令导航")
        cmd_nav_group_layout = QVBoxLayout(cmd_nav_group)
        
        self.cmd_tree = QTreeWidget()
        self.cmd_tree.setHeaderLabel("命令分类")
        self.load_command_tree()
        self.cmd_tree.itemClicked.connect(self.on_cmd_tree_clicked)
        cmd_nav_group_layout.addWidget(self.cmd_tree)
        
        # 添加命令按钮
        add_cmd_btn = QPushButton("添加命令")
        add_cmd_btn.clicked.connect(self.add_custom_command)
        cmd_nav_layout.addWidget(add_cmd_btn)
        
        cmd_nav_layout.addWidget(cmd_nav_group)
        
        middle_splitter.addWidget(cmd_nav_widget)
        middle_splitter.setSizes([350, 750])
        
        # 右侧数据区域
        data_widget = QWidget()
        data_layout = QVBoxLayout(data_widget)
        data_layout.setSpacing(10)
        data_layout.setContentsMargins(5, 5, 5, 5)
        
        # 命令信息
        cmd_info_group = QGroupBox("命令信息")
        cmd_info_layout = QVBoxLayout()
        
        self.cmd_info_text = QTextEdit()
        self.cmd_info_text.setReadOnly(True)
        self.cmd_info_text.setFixedHeight(100)
        cmd_info_layout.addWidget(self.cmd_info_text)
        cmd_info_group.setLayout(cmd_info_layout)
        data_layout.addWidget(cmd_info_group)
        
        # 数据输入
        data_input_group = QGroupBox("数据输入")
        data_input_layout = QVBoxLayout()
        
        self.data_input = QTextEdit()
        self.data_input.setPlaceholderText("输入数据（十六进制，空格分隔）")
        data_input_layout.addWidget(self.data_input)
        
        input_btn_layout = QHBoxLayout()
        clear_input_btn = QPushButton("清空")
        clear_input_btn.clicked.connect(lambda: self.data_input.clear())
        edit_btn = QPushButton("编辑")
        edit_btn.clicked.connect(self.open_command_editor)
        input_btn_layout.addWidget(clear_input_btn)
        input_btn_layout.addWidget(edit_btn)
        data_input_layout.addLayout(input_btn_layout)
        
        data_input_group.setLayout(data_input_layout)
        data_layout.addWidget(data_input_group)
        
        # 发送按钮
        send_btn = QPushButton("发送")
        send_btn.clicked.connect(self.send_data)
        send_btn.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
        data_layout.addWidget(send_btn)
        
        middle_splitter.addWidget(data_widget)
        main_layout.addWidget(middle_splitter)
        
        # 底部显示区域
        bottom_splitter = QSplitter(Qt.Vertical)
        
        # 响应显示
        response_group = QGroupBox("响应")
        response_layout = QVBoxLayout()
        response_layout.setSpacing(8)
        
        response_btn_layout = QHBoxLayout()
        self.analyze_btn = QPushButton("分析")
        self.analyze_btn.clicked.connect(self.open_response_analyzer)
        self.clear_response_btn = QPushButton("清空")
        self.clear_response_btn.clicked.connect(lambda: self.response_text.clear())
        
        response_btn_layout.addWidget(self.analyze_btn)
        response_btn_layout.addWidget(self.clear_response_btn)
        response_btn_layout.addStretch()
        
        response_layout.addLayout(response_btn_layout)
        
        self.response_text = QTextEdit()
        self.response_text.setReadOnly(True)
        response_layout.addWidget(self.response_text)
        response_group.setLayout(response_layout)
        bottom_splitter.addWidget(response_group)
        
        # 存储最后收到的数据
        self.last_received_data = b''
        
        # 调试日志
        log_group = QGroupBox("调试日志")
        log_layout = QVBoxLayout()
        log_layout.setSpacing(8)
        
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setFont(QFont("Courier New", 10))
        log_layout.addWidget(self.log_text)
        log_group.setLayout(log_layout)
        bottom_splitter.addWidget(log_group)
        
        bottom_splitter.setSizes([250, 150])
        main_layout.addWidget(bottom_splitter)
        
        # 状态栏
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("就绪")
    
    def refresh_ports(self):
        """
        刷新串口列表
        """
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combo.addItem(f"{port.device} - {port.description}")
    
    def toggle_connection(self):
        """
        切换串口连接状态
        """
        if self.serial_thread and self.serial_thread.isRunning():
            # 断开连接
            self.serial_thread.stop()
            self.serial_thread.wait()
            self.serial_thread = None
            self.connect_btn.setText("连接")
            self.status_bar.showMessage("已断开")
            self.log("串口已断开")
        else:
            # 建立连接
            if self.port_combo.currentText():
                port = self.port_combo.currentText().split(' ')[0]
                baudrate = int(self.baud_combo.currentText())
                
                self.serial_thread = SerialThread(port, baudrate)
                self.serial_thread.data_received.connect(self.on_data_received)
                self.serial_thread.error_occurred.connect(self.on_serial_error)
                self.serial_thread.start()
                
                self.connect_btn.setText("断开")
                self.status_bar.showMessage(f"已连接: {port} @ {baudrate}")
                self.log(f"串口已连接: {port} @ {baudrate}")
            else:
                QMessageBox.warning(self, "警告", "请选择串口")
    
    def set_protocol(self, protocol):
        """
        设置当前协议
        """
        self.current_protocol = protocol
        self.log(f"协议切换到: {protocol.upper()}")
    
    def toggle_rssi_filter(self):
        """
        切换RSSI过滤状态
        """
        self.rssi_filter_enabled = not self.rssi_filter_enabled
        if self.rssi_filter_enabled:
            self.rssi_filter_btn.setText("关闭RSSI过滤")
            self.log("RSSI过滤功能已开启，将忽略RSSI命令的数据")
        else:
            self.rssi_filter_btn.setText("开启RSSI过滤")
            self.log("RSSI过滤功能已关闭，将接收RSSI命令的数据")
    
    def load_command_tree(self):
        """
        加载命令树
        """
        self.cmd_tree.clear()
        
        categories = command_parser.get_all_categories()
        for category in categories:
            category_item = QTreeWidgetItem([category['name']])
            category_item.setData(0, Qt.UserRole, category['key'])
            
            # 添加该分类下的命令
            commands = command_parser.get_commands_by_category(category['key'])
            for cmd in commands:
                cmd_item = QTreeWidgetItem([cmd['name']])
                cmd_item.setData(0, Qt.UserRole, cmd['id'])
                cmd_item.setData(1, Qt.UserRole, cmd)
                category_item.addChild(cmd_item)
            
            self.cmd_tree.addTopLevelItem(category_item)
            category_item.setExpanded(True)
    
    def on_cmd_tree_clicked(self, item, column):
        """
        命令树点击事件
        """
        cmd_data = item.data(1, Qt.UserRole)
        if cmd_data:
            self.current_cmd_id = cmd_data['id']
            self.update_cmd_info(cmd_data)
    
    def update_cmd_info(self, cmd_data):
        """
        更新命令信息
        """
        info = f"命令: {cmd_data['name']}\n"
        info += f"ID: {cmd_data['hex']} ({cmd_data['id']})\n"
        info += f"描述: {cmd_data['description']}\n"
        self.cmd_info_text.setText(info)
    
    def send_data(self):
        """
        发送数据
        """
        if not self.serial_thread or not self.serial_thread.isRunning():
            QMessageBox.warning(self, "警告", "请先连接串口")
            return
        
        if not self.current_cmd_id:
            QMessageBox.warning(self, "警告", "请选择命令")
            return
        
        # 解析数据输入
        data_str = self.data_input.toPlainText().strip()
        data = b''
        if data_str:
            try:
                data = bytes.fromhex(data_str.replace(' ', ''))
            except Exception as e:
                QMessageBox.warning(self, "错误", f"数据格式错误: {e}")
                return
        
        # 构建帧
        if self.current_protocol == 'uart':
            # UART协议
            # 根据选择的方向设置sync值
            selected_direction = self.direction_group.checkedButton().text()
            if selected_direction == "SOC → MCU":
                sync = 0xAB  # SOC→MCU
                feature = 0xFF01
            else:
                sync = 0xBA  # MCU→SOC
                feature = 0xFF02
            # 添加seq字段：data = seq(1B) + payload(...)
            uart_data = self.seq_num.to_bytes(1, 'big') + data
            frame = SocMcu_Frame_Build(sync, feature, self.current_cmd_id, uart_data)
            self.seq_num = (self.seq_num + 1) % 256
        else:
            # BLE协议
            crypto = 0x00  # 不加密
            frame = build_ble_frame(self.current_cmd_id, data, crypto, self.seq_num)
            self.seq_num = (self.seq_num + 1) % 256
        
        # 发送数据
        if self.serial_thread.send(frame):
            self.log(f"发送: {format_frame_hex(frame)}")
            self.status_bar.showMessage(f"已发送 {len(frame)} 字节")
        else:
            self.log("发送失败")
    
    def on_data_received(self, data):
        """
        收到数据
        """
        # 存储最后收到的数据
        self.last_received_data = data
        
        # 解析数据
        if self.current_protocol == 'uart':
            # UART协议解析
            parsed = SocMcu_Frame_Parse(data)  # 注意：这里使用的是uart_protocol.py中的解析函数
        else:
            # BLE协议解析
            parsed = parse_ble_frame_ble(data)
        
        if parsed:
            # 检查是否需要过滤RSSI数据
            cmd_id = parsed.get('cmd_id', parsed.get('cmd', 0))
            if self.rssi_filter_enabled and cmd_id == 0x118:
                # 过滤RSSI数据，不进行处理，也不打印日志
                return
            
            # 生成详细的日志信息
            if self.current_protocol == 'uart':
                # UART协议详细日志
                direction = parsed.get('direction', '未知')
                sync = parsed.get('sync_hex', '0x00')
                feature = parsed.get('feature_hex', '0x0000')
                cmd_id = parsed.get('cmd_id_hex', '0x00')
                cmd_name = parsed.get('cmd_name', '未知命令')
                data_len = parsed.get('data_len', 0)
                data_hex = parsed.get('data_hex', '')
                crc_valid = "有效" if parsed.get('crc_valid', False) else "无效"
                crc_hex = parsed.get('crc_hex', '0x0000')
                total_len = parsed.get('total_len', 0)
                
                # 分析通道类型
                channel_type = "未知"
                if feature == "0xFF01":
                    channel_type = "CMD"
                elif feature == "0xFF02":
                    channel_type = "RESP"
                elif feature == "0xFF03":
                    channel_type = "SETTINGS"
                elif feature == "0xFF04":
                    channel_type = "RESP_SETTINGS"
                
                # 分析数据内容（如果是响应）
                result_code = ""
                result_desc = ""
                payload_data = ""
                if channel_type == "RESP" and data_len >= 2:
                    # 响应数据格式：seq(1B) + result(1B) + payload(...)
                    data_bytes = parsed.get('data', b'')
                    if data_bytes:
                        result_code = f"0x{data_bytes[1]:02X}"
                        if data_bytes[1] == 0x00:
                            result_desc = "成功"
                        elif data_bytes[1] == 0x01:
                            result_desc = "失败"
                        elif data_bytes[1] == 0x02:
                            result_desc = "参数错误"
                        elif data_bytes[1] == 0x03:
                            result_desc = "不支持"
                        elif data_bytes[1] == 0x04:
                            result_desc = "忙"
                        elif data_bytes[1] == 0x05:
                            result_desc = "状态错误"
                        if data_len > 2:
                            payload_data = format_frame_hex(data_bytes[2:])
                
                # 生成结构化日志
                log_msg = f"[{direction}] [{channel_type}] 命令: {cmd_name} ({cmd_id}) | "
                log_msg += f"sync={sync} | feature={feature} | "
                if result_code:
                    log_msg += f"result={result_code} ({result_desc}) | "
                log_msg += f"data_len={data_len} | "
                if payload_data:
                    log_msg += f"payload={payload_data} | "
                elif data_hex:
                    log_msg += f"data={data_hex} | "
                log_msg += f"crc={crc_hex} ({crc_valid}) | total_len={total_len}"
                
                # 显示解析结果
                response_text = f"解析结果:\n"
                response_text += f"方向: {direction}\n"
                response_text += f"通道: {channel_type} ({feature})\n"
                response_text += f"命令: {cmd_name} ({cmd_id})\n"
                response_text += f"Sync: {sync}\n"
                if result_code:
                    response_text += f"结果: {result_code} ({result_desc})\n"
                response_text += f"数据长度: {data_len} 字节\n"
                if payload_data:
                    response_text += f"载荷数据: {payload_data}\n"
                elif data_hex:
                    response_text += f"数据: {data_hex}\n"
                response_text += f"CRC校验: {crc_hex} ({crc_valid})\n"
                response_text += f"总长度: {total_len} 字节\n"
                response_text += f"原始数据: {format_frame_hex(data)}"
            else:
                # BLE协议日志
                cmd = parsed.get('cmd', 0)
                from protocol.ble_protocol import get_ble_command_name
                cmd_name = get_ble_command_name(cmd)
                payload = parsed.get('payload', b'')
                payload_len = len(payload)
                
                log_msg = f"[BLE] 命令: {cmd_name} (0x{cmd:04X}) | 数据长度: {payload_len} 字节"
                
                response_text = f"解析结果:\n"
                response_text += f"命令: {cmd_name} (0x{cmd:04X})\n"
                if payload_len > 0:
                    response_text += f"负载: {format_frame_hex(payload)}\n"
                response_text += f"原始数据: {format_frame_hex(data)}"
            
            # 输出日志
            self.log(log_msg)
            
            # 更新界面显示
            self.response_text.setText(response_text)
            
            # 自动模拟回复
            if self.simulate_response.isChecked():
                self.simulate_response_to_command(parsed)
        else:
            # 解析失败，显示原始数据
            log_msg = f"[解析失败] 原始数据: {format_frame_hex(data)}"
            self.log(log_msg)
            self.response_text.setText(f"原始数据: {format_frame_hex(data)}")
    
    def simulate_response_to_command(self, parsed_data):
        """
        模拟回复命令
        
        参数：
            parsed_data: 解析后的命令数据
        """
        try:
            # 获取命令ID
            if 'cmd_id' in parsed_data:
                cmd_id = parsed_data['cmd_id']
                data = parsed_data.get('data', b'')
                cmd_name = parsed_data.get('cmd_name', f"命令 0x{cmd_id:02X}")
                direction = parsed_data.get('direction', '未知')
            elif 'cmd' in parsed_data:
                cmd_id = parsed_data['cmd']
                data = parsed_data.get('payload', b'')
                from protocol.ble_protocol import get_ble_command_name
                cmd_name = get_ble_command_name(cmd_id)
                direction = "BLE"
            else:
                self.log("模拟回复: 无法获取命令ID")
                return
            
            # 分析命令类型和数据
            analysis_msg = f"[分析] 命令: {cmd_name} ({hex(cmd_id)}) | "
            analysis_msg += f"方向: {direction} | "
            analysis_msg += f"数据长度: {len(data)} 字节 | "
            if data:
                analysis_msg += f"数据: {format_frame_hex(data)}"
            else:
                analysis_msg += "无数据"
            self.log(analysis_msg)
            
            # 获取模拟响应
            response_data = response_simulator.get_response(cmd_id, data)
            
            # 构建响应帧
            if self.current_protocol == 'uart':
                # UART协议响应
                sync = 0xBA  # MCU→SOC
                feature = 0xFF02  # 命令应答通道
                
                # 提取seq（如果有的话）
                seq = 0
                if len(data) > 0:
                    seq = data[0]
                
                # 构建标准响应数据：seq(1B) + result(1B) + payload(...)
                # 响应数据的第一个字节是0x00（成功），后面是具体数据
                if response_data and response_data[0] != 0x00:
                    # 如果模拟器返回的数据不是以0x00开头，添加成功码
                    standard_response_data = seq.to_bytes(1, 'big') + b'\x00' + response_data
                else:
                    # 如果模拟器返回的数据已经以0x00开头，直接使用
                    standard_response_data = seq.to_bytes(1, 'big') + response_data
                
                response_frame = SocMcu_Frame_Build(sync, feature, cmd_id, standard_response_data)
                
                # 解析响应帧以便生成详细日志
                response_parsed = SocMcu_Frame_Parse(response_frame)
                if response_parsed:
                    resp_direction = response_parsed.get('direction', 'MCU→SOC')
                    resp_sync = response_parsed.get('sync_hex', '0xBA')
                    resp_feature = response_parsed.get('feature_hex', '0xFF02')
                    resp_cmd_id = response_parsed.get('cmd_id_hex', hex(cmd_id))
                    resp_data_len = response_parsed.get('data_len', len(standard_response_data))
                    resp_data_hex = response_parsed.get('data_hex', format_frame_hex(standard_response_data))
                    resp_crc_valid = "有效" if response_parsed.get('crc_valid', False) else "无效"
                    resp_crc_hex = response_parsed.get('crc_hex', '0x0000')
                    resp_total_len = response_parsed.get('total_len', len(response_frame))
                    
                    # 生成详细的回复日志
                    reply_log = f"[模拟回复] [{resp_direction}] 命令: {cmd_name} ({resp_cmd_id}) | "
                    reply_log += f"sync={resp_sync} | feature={resp_feature} | "
                    reply_log += f"data_len={resp_data_len} | data={resp_data_hex} | "
                    reply_log += f"crc={resp_crc_hex} ({resp_crc_valid}) | total_len={resp_total_len}"
                else:
                    reply_log = f"[模拟回复] 原始数据: {format_frame_hex(response_frame)}"
            else:
                # BLE协议响应
                crypto = 0x00  # 不加密
                response_frame = build_ble_frame(cmd_id, response_data, crypto, self.seq_num)
                self.seq_num = (self.seq_num + 1) % 256
                reply_log = f"[模拟回复] [BLE] 命令: {cmd_name} ({hex(cmd_id)}) | "
                reply_log += f"数据长度: {len(response_data)} 字节 | "
                if response_data:
                    reply_log += f"数据: {format_frame_hex(response_data)}"
                else:
                    reply_log += "无数据"
            
            # 发送响应
            if self.serial_thread and self.serial_thread.isRunning():
                if self.serial_thread.send(response_frame):
                    self.log(reply_log)
                    self.status_bar.showMessage(f"已发送模拟回复 {len(response_frame)} 字节")
                else:
                    self.log("模拟回复: 发送失败")
            else:
                self.log("模拟回复: 串口未连接")
        except Exception as e:
            self.log(f"模拟回复错误: {e}")
    
    def on_serial_error(self, error_msg):
        """
        串口错误
        """
        self.log(f"错误: {error_msg}")
        self.status_bar.showMessage(f"错误: {error_msg}")
    
    def log(self, message):
        """
        记录日志
        """
        from datetime import datetime
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.append(f"[{timestamp}] {message}")
        # 滚动到底部
        self.log_text.verticalScrollBar().setValue(self.log_text.verticalScrollBar().maximum())
    
    def add_custom_command(self):
        """
        添加自定义命令
        """
        # 创建添加命令对话框
        dialog = QDialog(self)
        dialog.setWindowTitle("添加自定义命令")
        dialog.setFixedWidth(400)
        dialog.setFixedHeight(300)
        
        layout = QVBoxLayout(dialog)
        
        # 命令ID
        id_layout = QHBoxLayout()
        id_layout.addWidget(QLabel("命令ID:"))
        id_input = QLineEdit()
        id_input.setPlaceholderText("例如: 0x01")
        id_layout.addWidget(id_input)
        layout.addLayout(id_layout)
        
        # 命令名称
        name_layout = QHBoxLayout()
        name_layout.addWidget(QLabel("命令名称:"))
        name_input = QLineEdit()
        name_input.setPlaceholderText("例如: 自定义命令")
        name_layout.addWidget(name_input)
        layout.addLayout(name_layout)
        
        # 命令描述
        desc_layout = QHBoxLayout()
        desc_layout.addWidget(QLabel("命令描述:"))
        desc_input = QLineEdit()
        desc_input.setPlaceholderText("例如: 这是一个自定义命令")
        desc_layout.addWidget(desc_input)
        layout.addLayout(desc_layout)
        
        # 按钮区域
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        
        cancel_btn = QPushButton("取消")
        cancel_btn.clicked.connect(dialog.reject)
        btn_layout.addWidget(cancel_btn)
        
        ok_btn = QPushButton("确定")
        ok_btn.clicked.connect(dialog.accept)
        btn_layout.addWidget(ok_btn)
        
        layout.addLayout(btn_layout)
        
        # 显示对话框
        if dialog.exec_() == QDialog.Accepted:
            # 获取输入的命令信息
            cmd_id_str = id_input.text().strip()
            cmd_name = name_input.text().strip()
            cmd_desc = desc_input.text().strip()
            
            # 验证输入
            if not cmd_id_str or not cmd_name:
                QMessageBox.warning(self, "警告", "请填写完整的命令信息")
                return
            
            try:
                # 解析命令ID
                if cmd_id_str.startswith('0x') or cmd_id_str.startswith('0X'):
                    cmd_id = int(cmd_id_str, 16)
                else:
                    cmd_id = int(cmd_id_str)
                
                # 添加到命令树
                self.add_command_to_tree(cmd_id, cmd_name, cmd_desc)
                
                QMessageBox.information(self, "成功", f"已添加命令: {cmd_name}")
            except ValueError:
                QMessageBox.warning(self, "错误", "命令ID格式错误，请使用十六进制格式（如0x01）")
    
    def add_command_to_tree(self, cmd_id, cmd_name, cmd_desc):
        """
        添加命令到命令树
        """
        # 查找或创建自定义分类
        custom_category = None
        for i in range(self.cmd_tree.topLevelItemCount()):
            item = self.cmd_tree.topLevelItem(i)
            if item.text(0) == "自定义命令":
                custom_category = item
                break
        
        # 如果不存在自定义分类，创建一个
        if not custom_category:
            custom_category = QTreeWidgetItem(["自定义命令"])
            custom_category.setData(0, Qt.UserRole, "custom")
            self.cmd_tree.addTopLevelItem(custom_category)
            custom_category.setExpanded(True)
        
        # 添加命令到自定义分类
        cmd_item = QTreeWidgetItem([cmd_name])
        cmd_item.setData(0, Qt.UserRole, cmd_id)
        cmd_item.setData(1, Qt.UserRole, {
            'id': cmd_id,
            'hex': f"0x{cmd_id:02X}",
            'name': cmd_name,
            'description': cmd_desc
        })
        custom_category.addChild(cmd_item)
    
    def open_command_editor(self):
        """
        打开命令编辑器
        """
        if not self.current_cmd_id:
            QMessageBox.warning(self, "警告", "请先选择命令")
            return
        
        # 获取当前命令信息
        cmd_info = command_parser.get_command_by_id(self.current_cmd_id)
        if not cmd_info:
            QMessageBox.warning(self, "警告", "命令信息不存在")
            return
        
        # 打开命令编辑器
        editor = CommandEditor(self, cmd_info)
        if editor.exec_() == QDialog.Accepted:
            # 获取编辑后的数据
            data = editor.get_data()
            # 转换为十六进制字符串并显示
            if data:
                hex_str = ' '.join(f'{b:02X}' for b in data)
                self.data_input.setText(hex_str)
            else:
                self.data_input.clear()
    
    def open_response_analyzer(self):
        """
        打开响应分析器
        """
        if not self.last_received_data:
            QMessageBox.warning(self, "警告", "没有收到数据")
            return
        
        # 打开响应分析器
        analyzer = ResponseAnalyzer(self, self.last_received_data, self.current_protocol)
        analyzer.exec_()
    
    def closeEvent(self, event):
        """
        关闭事件
        """
        if self.serial_thread:
            self.serial_thread.stop()
            self.serial_thread.wait()
        event.accept()

def main():
    """
    主函数
    """
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()
