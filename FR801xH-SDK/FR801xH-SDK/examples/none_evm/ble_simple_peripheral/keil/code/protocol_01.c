#include "protocol_01.h"
#include "co_printf.h"

/*
 * 说明：当前协议实现已按 Cmd 低字节分流 FE/FD。
 * 0x01xx（例如 0x0101）在文档中属于“设备 -> 手机”的回复包，
 * 不应该走“手机 -> 设备”的解析分发。
 *
 * 该文件保留为空实现，避免被误加入 Keil 工程时产生编译错误。
 */

void Protocol_Process_01(uint16_t cmd, uint8_t* payload, uint8_t len)
{
    (void)payload;
    (void)len;
    co_printf("Protocol: ignore 0x01xx cmd=0x%04X\r\n", cmd);
}
