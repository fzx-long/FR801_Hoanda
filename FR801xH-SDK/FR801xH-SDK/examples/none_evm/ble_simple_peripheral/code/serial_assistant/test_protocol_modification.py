#!/usr/bin/env python3
# 测试修改后的协议功能

from protocol.uart_protocol import SocMcu_Frame_Build, SocMcu_Frame_Parse, SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_CMD

# 测试命令208（设备主动同步数据）
def test_frame_build_parse():
    print("测试协议帧构建和解析：")
    
    # 测试数据：模拟设备主动同步数据
    test_data = bytes([
        0x01, 0x3C, 0x64, 0x01, 0x0A, 0x01, 0x1E, 0x1E, 0x01, 0x0A, 0x01, 0x0A, 0x01, 0x01, 0xFF, 0x00, 
        0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xEF, 0xFC, 0x01, 0x0A, 0x0B, 0x0B, 0x0A, 0x01, 0x02, 
        0x64, 0x7D, 0x96, 0xFF, 0xFC, 0x32, 0x02, 0x00, 0x64, 0x50, 0x02
    ])
    
    print(f"测试数据长度: {len(test_data)} bytes")
    print(f"测试数据: {test_data.hex()}")
    
    # 构建帧
    frame = SocMcu_Frame_Build(SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_CMD, 0x208, test_data)
    print(f"构建的帧长度: {len(frame)} bytes")
    print(f"构建的帧: {frame.hex()}")
    
    # 解析帧
    parsed = SocMcu_Frame_Parse(frame)
    if parsed:
        print("\n解析结果：")
        print(f"命令ID: {parsed['cmd_id_hex']} ({parsed['cmd_name']})")
        print(f"方向: {parsed['direction']}")
        print(f"同步字: {parsed['sync_hex']}")
        print(f"Feature: {parsed['feature_hex']}")
        print(f"Len字段值: {parsed['raw_frame'][2] << 8 | parsed['raw_frame'][3]} (0x{(parsed['raw_frame'][2] << 8 | parsed['raw_frame'][3]):04X})")
        print(f"数据长度: {len(parsed['data'])} bytes")
        print(f"CRC有效: {'是' if parsed['crc_valid'] else '否'}")
        print(f"总长度: {parsed['total_len']} bytes")
        
        # 验证数据一致性
        if parsed['data'] == test_data:
            print("\n✅ 测试通过：构建和解析的数据一致！")
        else:
            print("\n❌ 测试失败：构建和解析的数据不一致！")
    else:
        print("\n❌ 测试失败：无法解析构建的帧！")

# 测试空数据帧
def test_empty_data_frame():
    print("\n\n测试空数据帧：")
    
    # 构建空数据帧
    frame = SocMcu_Frame_Build(SOC_MCU_SYNC_SOC_TO_MCU, SOC_MCU_FEATURE_CMD, 0x209, None)
    print(f"构建的空数据帧长度: {len(frame)} bytes")
    print(f"构建的空数据帧: {frame.hex()}")
    
    # 解析帧
    parsed = SocMcu_Frame_Parse(frame)
    if parsed:
        print(f"解析结果 - 数据长度: {len(parsed['data'])} bytes")
        print(f"CRC有效: {'是' if parsed['crc_valid'] else '否'}")
        print("✅ 空数据帧测试通过！")
    else:
        print("❌ 空数据帧测试失败：无法解析构建的帧！")

if __name__ == "__main__":
    test_frame_build_parse()
    test_empty_data_frame()
    print("\n\n🎉 所有测试完成！")
