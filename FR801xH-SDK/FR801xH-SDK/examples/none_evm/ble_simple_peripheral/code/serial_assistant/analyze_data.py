#!/usr/bin/env python3
# 分析日志中的数据长度

data_hex = '00 00 01 3C 64 01 0A 01 1E 1E 01 0A 01 0A 01 01 FF 00 00 00 FF 00 00 00 FF EF FC 01 0A 0B 0B 0A 01 02 64 7D 96 FF FC 32 02 00 64 50 02'
data_bytes = bytes.fromhex(data_hex.replace(' ', ''))

print(f'Data length: {len(data_bytes)} bytes')
print(f'First 2 bytes (seq/result): {data_bytes[:2].hex()}')
print(f'Remaining data length: {len(data_bytes[2:])} bytes')
print(f'Remaining data: {data_bytes[2:].hex()}')

# 验证模拟器返回的数据长度
expected_data_len = 43  # 实际数据长度
expected_total_len = 2 + expected_data_len  # seq(1) + result(1) + data(43)

print(f'\nExpected data length: {expected_data_len} bytes')
print(f'Expected total length: {expected_total_len} bytes')
print(f'Actual total length: {len(data_bytes)} bytes')
print(f'Length matches: {len(data_bytes) == expected_total_len}')
