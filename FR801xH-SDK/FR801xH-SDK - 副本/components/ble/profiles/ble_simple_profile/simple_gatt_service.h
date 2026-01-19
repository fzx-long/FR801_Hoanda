/**
 * Copyright (c) 2019, Freqchip
 * 
 * All rights reserved.
 * 
 * 
 */

#ifndef SP_GATT_PROFILE_H
#define SP_GATT_PROFILE_H

/*
 * INCLUDES (����ͷ�ļ�)
 */
#include <stdio.h>
#include <string.h>
#include "gap_api.h"
#include "gatt_api.h"
#include "gatt_sig_uuid.h"


/*
 * MACROS (�궨��)
 */

/*
 * CONSTANTS (��������)
 */
// Simple Profile attributes index. 
enum
{
    SP_IDX_SERVICE,

    SP_IDX_CHAR1_DECLARATION,
    SP_IDX_CHAR1_VALUE,
	SP_IDX_CHAR1_CFG,
    SP_IDX_CHAR1_USER_DESCRIPTION,

    SP_IDX_CHAR2_DECLARATION,
    SP_IDX_CHAR2_VALUE,
    SP_IDX_CHAR2_CFG,
    SP_IDX_CHAR2_USER_DESCRIPTION,

    SP_IDX_CHAR3_DECLARATION,
    SP_IDX_CHAR3_VALUE,
    SP_IDX_CHAR3_USER_DESCRIPTION,
    
    SP_IDX_NB,
};

// Simple GATT Profile Service UUID
#define SP_SVC_UUID              0xFFF0

// User Request: Strictly follow 128-bit UUID definition based on Base:
// 0000xxxx-0000-1000-8000-00805F9B34FB
// 128-bit UUID = Base + (16-bit << 96) [Usually implemented as Little Endian Array]
// Little Endian: FB 34 9B 5F 80 00 00 80 00 10 00 00 [xx xx] 00 00
#define SP_SVC_UUID_128         {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xF0, 0xFF, 0x00, 0x00}
#define SP_CHAR1_UUID_128       {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xF6, 0xFF, 0x00, 0x00}
#define SP_CHAR2_UUID_128       {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xF1, 0xFF, 0x00, 0x00}
#define SP_CHAR3_UUID_128       {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x04, 0x10, 0x00, 0x00}

#define SP_CHAR1_TX_UUID            SP_CHAR1_UUID_128
#define SP_CHAR2_RX_UUID            SP_CHAR2_UUID_128
#define SP_CHAR3_UUID            0x1004

/*
 * TYPEDEFS (���Ͷ���)
 */

/*
 * GLOBAL VARIABLES (ȫ�ֱ���)
 */
extern uint8_t sp_svc_id;

/**
 * @brief 向指定连接发送通知
 */
void ntf_data(uint8_t con_idx, uint8_t att_idx, uint8_t *data, uint16_t len);

/**
 * @brief 向所有已连接且已订阅通知的设备发送通知
 * @param att_idx  特征值索引 (SP_IDX_CHAR1_VALUE 或 SP_IDX_CHAR2_VALUE)
 * @param data     数据指针
 * @param len      数据长度
 */
void ntf_data_all(uint8_t att_idx, uint8_t *data, uint16_t len);

/**
 * @brief 判断指定连接是否已开启 CHAR1 通知
 */
bool sp_is_char1_ntf_enabled(uint8_t con_idx);

/**
 * @brief 判断指定连接是否已开启 CHAR2 通知
 */
bool sp_is_char2_ntf_enabled(uint8_t con_idx);

/*
 * LOCAL VARIABLES (���ر���)
 */


/*
 * PUBLIC FUNCTIONS (ȫ�ֺ���)
 */
/*********************************************************************
 * @fn      sp_gatt_add_service
 *
 * @brief   Simple Profile add GATT service function.
 *			����GATT service��ATT�����ݿ����档
 *
 * @param   None. 
 *        
 *
 * @return  None.
 */
void sp_gatt_add_service(void);



#endif







