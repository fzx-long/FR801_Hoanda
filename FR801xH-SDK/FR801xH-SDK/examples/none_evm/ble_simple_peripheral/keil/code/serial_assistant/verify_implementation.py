#!/usr/bin/env python3
# 验证模拟器和协议实现的一致性

from protocol.simulator import response_simulator
from protocol.uart_protocol import build_response_frame, RESULT_SUCCESS

# 测试命令208（设备主动同步数据）
def test_command_208():
    print("测试命令208（设备主动同步数据）:")
    
    # 获取模拟器响应数据
    payload = response_simulator.get_response(0x208)
    print(f"模拟器返回数据长度: {len(payload)} bytes")
    print(f"模拟器返回数据: {payload.hex()}")
    
    # 构建完整响应帧
    response_frame = build_response_frame(0x208, 0, RESULT_SUCCESS, payload)
    print(f"构建的响应帧长度: {len(response_frame)} bytes")
    print(f"构建的响应帧: {response_frame.hex()}")
    
    # 验证总数据长度
    total_data_len = 2 + len(payload)  # seq(1) + result(1) + payload
    print(f"总数据段长度: {total_data_len} bytes")
    print(f"期望总数据段长度: 45 bytes")
    print(f"长度验证: {'通过' if total_data_len == 45 else '失败'}")
    print()

# 测试命令209（设备状态查询）
def test_command_209():
    print("测试命令209（设备状态查询）:")
    
    # 获取模拟器响应数据
    payload = response_simulator.get_response(0x209)
    print(f"模拟器返回数据长度: {len(payload)} bytes")
    print(f"模拟器返回数据: {payload.hex()}")
    
    # 构建完整响应帧
    response_frame = build_response_frame(0x209, 0, RESULT_SUCCESS, payload)
    print(f"构建的响应帧长度: {len(response_frame)} bytes")
    print(f"构建的响应帧: {response_frame.hex()}")
    
    # 验证总数据长度
    total_data_len = 2 + len(payload)  # seq(1) + result(1) + payload
    print(f"总数据段长度: {total_data_len} bytes")
    print(f"期望总数据段长度: 6 bytes")
    print(f"长度验证: {'通过' if total_data_len == 6 else '失败'}")
    print()

if __name__ == "__main__":
    test_command_208()
    test_command_209()
    print("实现一致性验证完成！")
