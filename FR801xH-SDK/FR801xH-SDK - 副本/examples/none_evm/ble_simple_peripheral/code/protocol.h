#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h> // 必须包含此头文件才能使用 bool

/* 开发阶段关闭 ACK/重传，可设为 1 打开 */
#define PROTOCOL_USE_ACK 0

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

void Protocol_Init(void);
void Protocol_Handle_Data(uint8_t conidx, uint8_t* data, uint16_t len);
void Protocol_Create_Test_Data(void);
void Protocol_Create_Test_FE_Connect_01FE(void);

/**
 * @brief 获取当前正在解析的连接索引（由 Protocol_Handle_Data 设置）
 */
uint8_t Protocol_Get_Rx_Conidx(void);

/**
 * @brief 鉴权状态管理（按连接）
 */
void Protocol_Auth_Clear(uint8_t conidx);
void Protocol_Auth_Set(uint8_t conidx, bool ok);
bool Protocol_Auth_IsOk(uint8_t conidx);

/**
 * @brief 记录“当前连接最后一次收包来自哪个特征值(CHAR)”
 * @note 用于实现：回包从同一条 Notify 通道发回（例如 APP 只订阅了 CHAR2）。
 *       仅支持 SP_IDX_CHAR1_VALUE / SP_IDX_CHAR2_VALUE / SP_IDX_CHAR3_VALUE；
 *       若为 CHAR3（仅写入，无 Notify），发送侧会回退到可通知的通道。
 */
void Protocol_Set_Rx_AttIdx(uint8_t conidx, uint16_t att_idx);

/**
 * @brief 回传鉴权结果（通过 Notify 通道）
 * @note 当前实现：按协议组完整 0x0101 帧，通过“最后一次收包所在的通道(CHAR1/CHAR2)”发送
 */
void Protocol_Auth_SendResult(uint8_t conidx, bool ok);

/**
 * @brief 主动断开指定连接
 */
void Protocol_Disconnect(uint8_t conidx);

/**
 * @brief 业务层发送带自动 ACK/重传的广播（逐连接通知）
 * @param cmd       命令标识
 * @param payload   负载指针
 * @param len       负载长度（不含协议头尾）
 * @param critical  是否关键数据（重传 3 次仍失败则断链）
 * @return 0 成功发起；负数表示忙或长度非法
 */
int Protocol_Send_Broadcast(uint16_t cmd, const uint8_t *payload, uint16_t len, bool critical);

/**
 * @brief 业务层发送“单播”通知（仅对指定 conidx）
 * @note
 * - 自动填写 Length/BCC/Header/Footer/流水号
 * - 发送通道会按“该 conidx 最近一次 RX 的特征值”选择 CHAR1/CHAR2
 * - 若 notify 未开启，会返回错误
 * @param conidx    连接索引
 * @param cmd       命令标识
 * @param payload   负载指针（可为 NULL，表示无数据段）
 * @param len       负载长度（不含协议头尾）
 * @return 0 成功；负数表示长度非法/未连接/notify 未开启等
 */
int Protocol_Send_Unicast(uint8_t conidx, uint16_t cmd, const uint8_t *payload, uint16_t len);

/**
 * @brief 业务层发送“单播”通知（仅对指定 conidx），并强制使用新的流水号
 * @details
 * - 用于设备主动推送类命令（例如 0x64FD/0x66FD 参数同步）
 * - 为什么需要：Protocol_Send_Unicast 会倾向复用 last_rx_seq（用于应答回显 seq），
 *   但主动推送若复用旧 seq，APP 可能把它当成上一个请求的应答而丢弃。
 */
int Protocol_Send_Unicast_Async(uint8_t conidx, uint16_t cmd, const uint8_t *payload, uint16_t len);

/*
 * 若 payload 是编译期已知大小的数组，可用此宏自动计算 len：
 *  Protocol_Send_Broadcast_ARRAY(cmd, array, critical);
 * 注意：必须传数组标识符本身，不能传指针，否则 sizeof 得到的是指针大小。
 */
#define Protocol_Send_Broadcast_ARRAY(cmd, array, critical) \
    Protocol_Send_Broadcast((cmd), (array), (uint16_t)sizeof(array), (critical))

#endif // __PROTOCOL_H__

