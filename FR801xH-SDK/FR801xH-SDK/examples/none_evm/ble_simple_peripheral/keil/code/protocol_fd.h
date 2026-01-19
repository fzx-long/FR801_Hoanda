#ifndef __PROTOCOL_FD_H__
#define __PROTOCOL_FD_H__

#include <stdint.h>

void Protocol_Process_FD(uint16_t cmd, uint8_t* payload, uint8_t len);

#endif // __PROTOCOL_FD_H__
