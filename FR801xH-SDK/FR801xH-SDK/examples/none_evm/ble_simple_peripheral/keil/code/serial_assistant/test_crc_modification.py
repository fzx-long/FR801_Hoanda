#!/usr/bin/env python3
# 测试CRC校验修改是否正确

from protocol.uart_protocol import SocMcu_Frame_Build, SocMcu_Frame_Parse, SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_CMD

print("测试CRC校验修改：")

# 测试数据1：简单数据
print("\n1. 测试简单数据帧：")
data1 = b'\x01\x02\x03\x04'
frame1 = SocMcu_Frame_Build(SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_CMD, 0x100, data1)
print(f"构建的帧: {frame1.hex()}")
print(f"帧长度: {len(frame1)}")

parsed1 = SocMcu_Frame_Parse(frame1)
if parsed1:
    print(f"解析成功: {parsed1['cmd_id_hex']}")
    print(f"CRC有效: {'是' if parsed1['crc_valid'] else '否'}")
    print(f"解析数据: {parsed1['data'].hex()}")
else:
    print("解析失败")

# 测试数据2：设备同步数据
print("\n2. 测试设备同步数据帧：")
data2 = bytes([
    0x01, 0x3C, 0x64, 0x01, 0x0A, 0x01, 0x1E, 0x1E, 0x01, 0x0A, 0x01, 0x0A, 0x01, 0x01, 0xFF, 0x00, 
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xEF, 0xFC, 0x01, 0x0A, 0x0B, 0x0B, 0x0A, 0x01, 0x02, 
    0x64, 0x7D, 0x96, 0xFF, 0xFC, 0x32, 0x02, 0x00, 0x64, 0x50, 0x02
])
frame2 = SocMcu_Frame_Build(SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_CMD, 0x208, data2)
print(f"构建的帧: {frame2.hex()}")
print(f"帧长度: {len(frame2)}")

parsed2 = SocMcu_Frame_Parse(frame2)
if parsed2:
    print(f"解析成功: {parsed2['cmd_id_hex']}")
    print(f"CRC有效: {'是' if parsed2['crc_valid'] else '否'}")
    print(f"解析数据长度: {len(parsed2['data'])} bytes")
else:
    print("解析失败")

# 测试数据3：空数据
print("\n3. 测试空数据帧：")
data3 = b''
frame3 = SocMcu_Frame_Build(SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_CMD, 0x209, data3)
print(f"构建的帧: {frame3.hex()}")
print(f"帧长度: {len(frame3)}")

parsed3 = SocMcu_Frame_Parse(frame3)
if parsed3:
    print(f"解析成功: {parsed3['cmd_id_hex']}")
    print(f"CRC有效: {'是' if parsed3['crc_valid'] else '否'}")
    print(f"解析数据长度: {len(parsed3['data'])} bytes")
else:
    print("解析失败")

print("\n🎉 所有测试完成！")
