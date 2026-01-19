#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BLE协议处理器
基于protocol.c和protocol.h的协议规范实现
"""

import struct
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend

# 协议常量定义
PROTOCOL_HEADER_MAGIC = 0x5555
PROTOCOL_FOOTER_MAGIC = 0xAAAA

# 加密类型定义
CRYPTO_TYPE_NONE = 0x00
CRYPTO_TYPE_DES3 = 0x01
CRYPTO_TYPE_AES128 = 0x02

# 确认命令标识
CMD_ACK_ID = 0x0000

# 默认AES密钥和IV
DEFAULT_AES_KEY = b'QSDfagQ141GS6JF8'
DEFAULT_AES_IV = b'\x00' * 16

# 协议物理帧头结构
class Protocol_Header:
    def __init__(self):
        self.header = PROTOCOL_HEADER_MAGIC
        self.length = 0
        self.crypto = CRYPTO_TYPE_NONE
        self.seq_num = 0
        self.cmd = 0

def calculate_bcc(data):
    """
    计算BCC校验
    XOR所有字节
    """
    bcc = 0
    for byte in data:
        bcc ^= byte
    return bcc

def pkcs7_pad(data, block_size=16):
    """
    PKCS7填充
    """
    pad_len = block_size - (len(data) % block_size)
    return data + bytes([pad_len]) * pad_len

def pkcs7_unpad(data):
    """
    PKCS7去填充
    """
    if not data:
        return data
    pad_len = data[-1]
    if pad_len > len(data):
        return data
    if data[-pad_len:] == bytes([pad_len]) * pad_len:
        return data[:-pad_len]
    return data

def aes_encrypt(data, key=DEFAULT_AES_KEY, iv=DEFAULT_AES_IV):
    """
    AES-128-CBC加密
    """
    backend = default_backend()
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv), backend=backend)
    encryptor = cipher.encryptor()
    padded_data = pkcs7_pad(data)
    ciphertext = encryptor.update(padded_data) + encryptor.finalize()
    return ciphertext

def aes_decrypt(data, key=DEFAULT_AES_KEY, iv=DEFAULT_AES_IV):
    """
    AES-128-CBC解密
    """
    backend = default_backend()
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv), backend=backend)
    decryptor = cipher.decryptor()
    plaintext = decryptor.update(data) + decryptor.finalize()
    return pkcs7_unpad(plaintext)

def build_ble_frame(cmd, payload=b'', crypto=CRYPTO_TYPE_NONE, seq_num=0):
    """
    构建BLE协议帧
    
    参数：
        cmd: 命令ID
        payload: 负载数据
        crypto: 加密类型
        seq_num: 流水号
    
    返回：
        构建好的帧字节列表
    """
    # 处理加密
    if crypto == CRYPTO_TYPE_AES128 and payload:
        encrypted_payload = aes_encrypt(payload)
    else:
        encrypted_payload = payload
    
    # 计算头部长度
    header_len = 2 + 1 + 1 + 1 + 2  # header(2) + length(1) + crypto(1) + seq(1) + cmd(2)
    payload_len = len(encrypted_payload)
    total_len = header_len + payload_len + 1 + 2  # + BCC(1) + Footer(2)
    
    # 构建帧
    frame = bytearray(total_len)
    
    # 填充头部
    frame[0:2] = struct.pack('<H', PROTOCOL_HEADER_MAGIC)  # 小端序
    frame[2] = total_len
    frame[3] = crypto
    frame[4] = seq_num
    frame[5:7] = struct.pack('<H', cmd)  # 小端序
    
    # 填充负载
    if payload_len > 0:
        frame[7:7+payload_len] = encrypted_payload
    
    # 计算BCC
    bcc_data = frame[:7+payload_len]
    bcc = calculate_bcc(bcc_data)
    frame[7+payload_len] = bcc
    
    # 填充帧尾
    frame[-2:] = struct.pack('<H', PROTOCOL_FOOTER_MAGIC)  # 小端序
    
    return frame

def parse_ble_frame(frame):
    """
    解析BLE协议帧
    
    参数：
        frame: 接收到的帧字节列表
    
    返回：
        解析结果字典
        解析失败返回None
    """
    try:
        # 检查最小长度
        if len(frame) < 10:  # 最小帧长度
            return None
        
        # 检查帧头
        header = struct.unpack('<H', frame[0:2])[0]
        if header != PROTOCOL_HEADER_MAGIC:
            return None
        
        # 检查长度字段
        declared_len = frame[2]
        if declared_len != len(frame):
            return None
        
        # 检查帧尾
        footer = struct.unpack('<H', frame[-2:])[0]
        if footer != PROTOCOL_FOOTER_MAGIC:
            return None
        
        # 解析基本字段
        crypto = frame[3]
        seq_num = frame[4]
        cmd = struct.unpack('<H', frame[5:7])[0]
        
        # 提取负载和BCC
        payload_len = len(frame) - 10  # 总长度 - 头部(7) - BCC(1) - 尾部(2)
        if payload_len > 0:
            payload = frame[7:7+payload_len]
        else:
            payload = b''
        
        # 验证BCC
        bcc_data = frame[:7+payload_len]
        expected_bcc = frame[7+payload_len]
        actual_bcc = calculate_bcc(bcc_data)
        if actual_bcc != expected_bcc:
            return None
        
        # 解密负载
        if crypto == CRYPTO_TYPE_AES128 and payload:
            try:
                decrypted_payload = aes_decrypt(payload)
            except:
                decrypted_payload = payload
        else:
            decrypted_payload = payload
        
        # 返回解析结果
        return {
            'header': header,
            'length': declared_len,
            'crypto': crypto,
            'seq_num': seq_num,
            'cmd': cmd,
            'payload': decrypted_payload,
            'encrypted_payload': payload,
            'bcc': expected_bcc
        }
        
    except Exception:
        return None

def format_ble_frame_hex(frame):
    """
    将BLE帧格式化为十六进制字符串
    """
    return ' '.join(f'{b:02X}' for b in frame)

def format_ble_payload(payload):
    """
    格式化BLE负载
    """
    try:
        return payload.decode('utf-8', errors='replace')
    except:
        return ' '.join(f'{b:02X}' for b in payload)

# BLE命令定义
BLE_COMMANDS = {
    0x0000: "ACK命令",
    0x0101: "鉴权结果",
    0x01FE: "FE连接命令",
    0x01FD: "FD命令"
}

def get_ble_command_name(cmd):
    """
    获取BLE命令名称
    """
    return BLE_COMMANDS.get(cmd, f"未知命令 0x{cmd:04X}")
