/**
 * @file flash_op.h
 * @author fan (you@domain.com)
 * @brief 用于Flash操作的头文件
 * @version 0.1
 * @date 2026-01-05
 * 
 * @copyright Copyright (c) 2026
 * 
 */
/****************************Include***********************************/
#ifndef FLASH_OP_H
#define FLASH_OP_H

#include <stdint.h>
/*****************************Type Define*******************************/
/*********************************************************************
 * @brief TPMS 绑定信息示例结构体（可按实际协议扩展字段）。
 */
typedef struct {
    uint8_t sensor_id[4];
    uint8_t status;
    uint8_t reserved[3]; // 补齐对齐
} tpms_bind_info_t;
/*****************************Function Declare**************************/
/*********************************************************************
 * @brief 基础接口：读/写/擦除封装。
 */
void flash_op_read(uint32_t addr, uint8_t* buf, uint32_t len);
void flash_op_write_direct(uint32_t addr, uint8_t* buf, uint32_t len);
void flash_op_erase_sector(uint32_t addr);

/*********************************************************************
 * @brief 应用层接口：TPMS 绑定信息的保存/加载。
 */
void flash_op_save_tpms_info(uint32_t addr, tpms_bind_info_t* info);
void flash_op_load_tpms_info(uint32_t addr, tpms_bind_info_t* info);

#endif
