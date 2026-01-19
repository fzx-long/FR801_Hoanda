# 串口测试助手

一个基于Python和PyQt5的串口测试助手，专门用于测试SOC→MCU和MCU→SOC通信协议。

## 功能特点

- ✅ 支持UART协议和BLE协议
- ✅ 完整的命令导航和编辑功能
- ✅ 实时数据收发和解析
- ✅ 自动模拟MCU响应
- ✅ 详细的数据分析和日志
- ✅ 支持十六进制和ASCII数据输入
- ✅ 跨平台兼容（Windows、Linux、macOS）

## 系统要求

- Python 3.x
- PyQt5
- pyserial
- cryptography（用于AES加密）

## 安装方法

### 1. 克隆项目

```bash
git clone <项目地址>
cd serial_assistant
```

### 2. 安装依赖

```bash
# 使用pip安装
pip install pyqt5 pyserial cryptography

# 或使用conda
conda create -n rknn python=3.8
conda activate rknn
pip install pyqt5 pyserial cryptography
```

## 使用方法

### 基本操作

1. **启动程序**
   ```bash
   python main.py
   ```

2. **配置串口**
   - 选择正确的COM端口
   - 设置波特率（默认115200）
   - 点击"连接"按钮

3. **选择协议**
   - UART协议：用于SOC→MCU通信
   - BLE协议：用于蓝牙通信

4. **选择命令**
   - 在左侧命令导航树中选择命令分类
   - 点击具体命令查看详细信息

5. **输入数据**
   - 在"数据输入"区域输入十六进制数据（空格分隔）
   - 或点击"编辑"按钮打开可视化编辑器

6. **发送命令**
   - 点击"发送"按钮发送命令
   - 查看"响应"区域的返回结果

7. **分析数据**
   - 点击"分析"按钮打开详细的数据分析窗口
   - 查看原始数据、解析结果和数据分析

### 高级功能

#### 命令编辑器
- 支持十六进制和ASCII模式
- 提供常用数据模板
- 实时预览数据格式和长度

#### 响应分析器
- 原始数据查看
- 协议解析结果
- 详细的数据分析表格
- 数据保存功能

#### 模拟回复
- 启用"模拟回复"选项
- 系统会自动模拟MCU的响应
- 支持所有命令的响应处理

## 协议说明

### UART协议

**帧格式**：
- 头部：0xFE
- 同步字：0xAB（SOC→MCU）/ 0xBA（MCU→SOC）
- Feature：2字节（大端）
- 命令ID：2字节（大端）
- 长度：2字节（大端，表示ID+数据长度）
- 数据：可变长度
- CRC：2字节（CRC16-CCITT）
- 尾部：0x0A 0x0D

### BLE协议

**帧格式**：
- 头部：0x5555
- 长度：1字节（总长度）
- 加密类型：1字节（0x00=不加密，0x02=AES128）
- 流水号：1字节
- 命令：2字节（小端）
- 数据：可变长度（可加密）
- BCC：1字节（异或校验）
- 尾部：0xAAAA

## 项目结构

```
serial_assistant/
├── main.py              # 主程序入口
├── protocol/            # 协议实现
│   ├── uart_protocol.py    # UART协议处理器
│   ├── ble_protocol.py     # BLE协议处理器
│   ├── command_parser.py   # 命令解析器
│   └── simulator.py        # 响应模拟器
├── ui/                  # 界面实现
│   ├── main_window.py      # 主窗口
│   ├── command_editor.py   # 命令编辑器
│   └── response_analyzer.py # 响应分析器
└── data/                # 数据文件
    └── commands.json       # 命令数据库
```

## 命令分类

- **蓝牙相关命令**：连接、配对、重置等
- **NFC相关命令**：钥匙管理、开关控制等
- **车辆控制命令**：解锁、锁车、防盗等
- **智能设置命令**：灯光、音效、模式设置等
- **传感器相关命令**：胎压监测等

## 常见问题

### 1. 串口连接失败
- 检查USB驱动是否安装
- 确认串口未被其他程序占用
- 检查波特率设置是否正确

### 2. 数据解析失败
- 确认选择了正确的协议
- 检查数据格式是否正确
- 查看日志窗口的详细信息

### 3. 模拟回复不工作
- 确认启用了"模拟回复"选项
- 检查串口是否已连接
- 查看日志窗口的回复状态

## 开发说明

### 添加新命令
1. 编辑 `data/commands.json` 文件
2. 在对应分类下添加新命令
3. 重新启动程序

### 自定义响应
1. 编辑 `protocol/simulator.py` 文件
2. 在 `ResponseSimulator` 类中添加新的命令处理方法
3. 在 `response_templates` 字典中注册命令

## 许可证

MIT License

## 联系方式

- 作者：Fanzx
- 邮箱：1456925916@qq.com
