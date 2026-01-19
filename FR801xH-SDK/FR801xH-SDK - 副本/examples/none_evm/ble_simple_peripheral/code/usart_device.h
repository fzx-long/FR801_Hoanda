#ifndef __USART_DEVICE__
#define __USART_DEVICE__

#include <stdint.h>

/**
 * @brief 构建 SOC↔MCU UART 二进制帧
 * @param sync    同步字（0xABAB 下发 / 0xBABA 回包）
 * @param feature 通道字段（0xFF01~0xFF04）
 * @param id      命令 ID（2 字节，小端编码进帧）
 * @param data    数据段指针（可为 NULL，当 data_len=0）
 * @param data_len 数据段长度
 * @param out     输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @return 构建出的帧长度；失败返回 0
 */
uint16_t SocMcu_Frame_Build(uint16_t sync,
                            uint16_t feature,
                            uint16_t id,
                            const uint8_t *data,
                            uint16_t data_len,
                            uint8_t *out,
                            uint16_t out_cap);

/**
 * @brief 发送 SOC↔MCU UART 二进制帧（走 g_uart1_dev.send）
 * @return 1=发送成功；0=失败
 */
uint8_t SocMcu_Frame_Send(uint16_t sync,
                          uint16_t feature,
                          uint16_t id,
                          const uint8_t *data,
                          uint16_t data_len);

// 前向声明
struct UART_Comm_Base_s;

typedef struct {
    void (*init)(struct UART_Comm_Base_s *self);                    // 初始化
    void (*deinit)(struct UART_Comm_Base_s *self);                  // 反初始化
    void (*set_baud_rate)(struct UART_Comm_Base_s *self, uint32_t baud); // 设置波特率
} UART_Comm_Ops_t;

typedef struct UART_Comm_Base_s {
    uint32_t uart_port;                     // 串口端口号 (UART0, UART1)
    uint32_t baud_rate;                     // 波特率
    
    // 业务接口
    uint8_t (*send)(struct UART_Comm_Base_s *self, uint8_t *data, uint16_t len);    // 发送
    uint8_t (*receive)(struct UART_Comm_Base_s *self, uint8_t *data, uint16_t len); // 接收
    
    // 底层操作接口
    UART_Comm_Ops_t ops; 
} UART_Comm_Base_t;

// 构造函数声明
void UART_Device_Create(UART_Comm_Base_t *device, uint32_t port, uint32_t baud);
void UART_Task_Init(void);

// 全局设备实例声明
extern UART_Comm_Base_t g_uart1_dev;

#endif // !__USART_DEVICE__
