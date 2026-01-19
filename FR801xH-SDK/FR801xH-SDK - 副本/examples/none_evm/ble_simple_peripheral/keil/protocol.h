#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h> // 必须包含此头文件才能使用 bool

// 协议常量定义
#define PROTOCOL_HEADER_MAGIC   0x5555
#define PROTOCOL_FOOTER_MAGIC   0xAAAA

// 加密类型定义
#define CRYPTO_TYPE_NONE        0x00
#define CRYPTO_TYPE_DES3        0x01
#define CRYPTO_TYPE_AES128      0x02

// 确认命令标识
#define CMD_ACK_ID              0x0000

#pragma pack(push, 1) // 确保结构体按1字节对齐

// 1. 协议物理帧头 (对应协议的前7个字节，用于直接解析)
// 注意：不要在这里放指针，也不要放变长数据后面的字段(BCC/Footer)
typedef struct {
    uint16_t header;        // 固定为 0x5555
    uint8_t  length;        // 数据包总长度，包括 header、footer
    uint8_t  crypto;        // 加密算法
    uint8_t  seq_num;       // 流水号
    uint16_t cmd;           // 命令标识
} Protocol_Header_t;

#pragma pack(pop)

// 2. 协议处理对象 (用于逻辑控制，模拟面向对象)
// 前向声明，解决结构体内部指针引用自身的问题
typedef struct Protocol_Handler_s Protocol_Handler_t;

struct Protocol_Handler_s {
    // 数据缓存
    uint8_t* rx_buffer;     // 接收缓冲区指针
    uint16_t rx_len;        // 当前接收长度
    
    // 解析后的字段
    Protocol_Header_t header_info; // 头部信息
    uint8_t*          payload;     // 指向数据段的指针
    uint8_t           payload_len; // 数据段长度
    
    // 方法 (函数指针)
    bool (*Check_BCC)(Protocol_Handler_t* self);     // 校验BCC
    bool (*Parse)(Protocol_Handler_t* self);         // 解析包
};

// 全局变量声明 (使用 extern，不要在头文件直接定义变量)
extern Protocol_Handler_t g_protocol_handler;

/**
 * @brief 业务层发送“单播”通知（仅对指定 conidx），并强制使用新的流水号
 * @details
 * - 用于设备主动推送类命令（例如 0x64FD/0x66FD 参数同步）
 * - 为什么需要：Protocol_Send_Unicast 会倾向复用 last_rx_seq（用于应答回显 seq），
 *   但主动推送若复用旧 seq，APP 可能把它当成上一个请求的应答而丢弃。
 */
int Protocol_Send_Unicast_Async(uint8_t conidx, uint16_t cmd, const uint8_t *payload, uint16_t len);
#endif // __PROTOCOL_H__