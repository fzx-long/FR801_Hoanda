#!/usr/bin/env python3
# 简单测试脚本

from protocol.uart_protocol import SocMcu_Frame_Build, SocMcu_Frame_Parse, SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_CMD

# 构建一个简单的帧
data = b'\x01\x02\x03\x04'
frame = SocMcu_Frame_Build(SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_CMD, 0x100, data)

print(f"构建的帧: {frame.hex()}")
print(f"帧长度: {len(frame)}")
print(f"Len字段: {frame[2] << 8 | frame[3]} (0x{(frame[2] << 8 | frame[3]):04X})")
print(f"期望Len字段: {2+2+len(data)} (0x{(2+2+len(data)):04X})")

# 解析帧
parsed = SocMcu_Frame_Parse(frame)
if parsed:
    print(f"解析成功: {parsed['cmd_id_hex']}")
    print(f"解析数据: {parsed['data'].hex()}")
else:
    print("解析失败")
