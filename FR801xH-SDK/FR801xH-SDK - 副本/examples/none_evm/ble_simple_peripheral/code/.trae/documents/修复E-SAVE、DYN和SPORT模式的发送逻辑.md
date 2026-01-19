# 修复E-SAVE、DYN和SPORT模式的发送逻辑

## 问题分析

通过分析代码，发现以下问题：

1. **缺少命令映射**：在 `BleFunc_MapBleCmdToMcuId` 函数中，没有处理以下命令：
   - `set_E_SAVE_mode` (0x0cFD) - E-SAVE模式
   - `set_DYN_mode` (0x0dFD) - DYN模式
   - `set_sport_mode` (0x0eFD) - SPORT模式

2. **发送逻辑错误**：当前代码没有让这些模式直接发送它们的档位值，而是使用了复杂的控制逻辑。

## 解决方案

修改 `ble_function.c` 文件中的 `BleFunc_MapBleCmdToMcuId` 函数，添加对这些命令的处理，让它们直接发送档位值。

### 具体修改步骤

1. **在 `BleFunc_MapBleCmdToMcuId` 函数中添加以下处理逻辑**：

   - 对于 `set_E_SAVE_mode` (0x0cFD)：直接映射到对应的MCU命令
   - 对于 `set_DYN_mode` (0x0dFD)：直接映射到对应的MCU命令  
   - 对于 `set_sport_mode` (0x0eFD)：直接映射到对应的MCU命令

2. **确保这些命令直接发送档位值**，而不是通过复杂的控制逻辑。

### 修改后的预期行为

- 当接收到 `set_E_SAVE_mode` 命令时，直接发送E-SAVE模式的档位值
- 当接收到 `set_DYN_mode` 命令时，直接发送DYN模式的档位值
- 当接收到 `set_sport_mode` 命令时，直接发送SPORT模式的档位值

这样修改后，这些模式的发送逻辑将符合通用蓝牙协议的要求。