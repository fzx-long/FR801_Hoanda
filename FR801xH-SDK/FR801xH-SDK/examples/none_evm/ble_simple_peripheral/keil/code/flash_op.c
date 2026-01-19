/*********************************************************************
 * @file flash_op.c
 * @author Fanzx (1456925916@qq.com)
 * @brief 用于Flash操作的实现文件包括读写擦除等功能
 * @version 0.1
 * @date 2026-01-06
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "flash_op.h"
#include "driver_flash.h"
#include "flash_usage_config.h"

/*********************************************************************
 * @brief  从内部 Flash 读取数据（仅读，无需擦除）。
 * @param  addr  绝对地址（XIP 空间内偏移）。
 * @param  buf   目标缓存指针。
 * @param  len   读取字节数。
 */
void flash_op_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    flash_read(addr, len, buf);
}

/*********************************************************************
 * @brief  直接写入 Flash（调用前必须确保目标区域已被擦除为 0xFF）。
 * @param  addr  写入起始地址，建议页/扇区对齐。
 * @param  buf   数据源指针。
 * @param  len   写入字节数。
 * @note   Flash 写只能把位从 1->0，若未先擦除会导致数据“与”出错。
 */
void flash_op_write_direct(uint32_t addr, uint8_t *buf, uint32_t len)
{
    flash_write(addr, len, buf);
}

/*********************************************************************
 * @brief  擦除单个 4KB 扇区。
 * @param  addr  扇区起始地址，必须 0x1000 对齐。
 * @note   擦除会将整个扇区恢复为 0xFF，请确认无其他数据同扇区或已做好备份。
 */
void flash_op_erase_sector(uint32_t addr)
{
    // size 传 0x1000 表示擦除一个扇区
    flash_erase(addr, 0x1000);
}

/*********************************************************************
 * @brief  保存 TPMS 绑定信息（封装“先擦后写”）。
 * @param  addr  目标扇区起始地址，默认使用 TPMS_BINDING_INFO_SAVE_ADDR。
 * @param  info  待写入结构体指针。
 * @note   假定该扇区仅存此结构；若还有其他数据，需先整扇区读出再合并写回。
 */
void flash_op_save_tpms_info(uint32_t addr, tpms_bind_info_t *info)
{
    // 1. 擦除扇区
    // 注意：这里假设整个扇区只存这一个数据，或者你不介意丢失该扇区其他数据
    // 如果该扇区还有其他重要数据，必须先 Read-Modify-Write
    flash_op_erase_sector(addr);

    // 2. 写入新数据
    flash_op_write_direct(addr, (uint8_t *)info, sizeof(tpms_bind_info_t));
}

/*********************************************************************
 * @brief  加载 TPMS 绑定信息（仅读）。
 * @param  addr  绑定信息所在地址。
 * @param  info  输出结构体指针。
 */
void flash_op_load_tpms_info(uint32_t addr, tpms_bind_info_t *info)
{
    flash_op_read(addr, (uint8_t *)info, sizeof(tpms_bind_info_t));
}
