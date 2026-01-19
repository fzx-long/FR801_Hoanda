/**
 * @author fzx-long (1456925916@qq.com)
 * @brief  本文件用于处理回复手机相关功能
 * @version 0.1
 * @date 2025-12-31
 */
#ifndef PHONE_REPLY_H
#define PHONE_REPLY_H

#include <stdbool.h>
#include <stdint.h>

/* 协议帧固定开销：Header(2)+Len(1)+Crypto(1)+Seq(1)+Cmd(2)+BCC(1)+Footer(2)=10 */
#define PHONE_REPLY_FRAME_OVERHEAD        10u

/* 当前工程中“ModelMac”按 BLE 地址 6 字节处理 */
#define PHONE_REPLY_MODEL_MAC_MAX_LEN     6u

/*
 * 设计说明（为什么要放这里）：
 * - ble_function 负责“业务判断”(该不该回、回什么结果)
 * - phone_reply 负责“拼包/组帧”(按协议 5555..AAAA + BCC + 字节序)
 * - protocol 只负责“收包解析 + 发 Notify 通道”
 */

/**
 * @brief 通用：按协议格式组一帧（不发送，只负责拼包）
 * @param cmd         命令字（小端写入）
 * @param seq         流水号（由 protocol 层递增管理）
 * @param payload     数据段
 * @param payload_len 数据段长度
 * @param out_frame   输出帧缓冲区
 * @param out_cap     输出缓冲区容量
 * @param out_len     输出实际帧长
 * @return true 成功；false 失败（长度超限/缓冲区不足/参数错误）
 */
bool PhoneReply_BuildFrame(uint16_t cmd,
						   uint8_t seq,
						   const uint8_t *payload,
						   uint16_t payload_len,
						   uint8_t *out_frame,
						   uint16_t out_cap,
						   uint16_t *out_len);

/**
 * @brief 通用：按协议格式组一帧（支持指定 crypto；不发送，只负责拼包）
 * @note 为什么要新增：当前 App/协议要求“回包也需要加密”，而旧接口固定写 CRYPTO_TYPE_NONE。
 * @param cmd         命令字（小端写入）
 * @param crypto      加密算法（0x00/0x01/0x02），应与接收端解密逻辑一致
 * @param seq         流水号（由 protocol 层管理，通常需要回显请求的 seq）
 * @param payload     数据段（若 crypto!=NONE，通常传入“已经加密后的密文”）
 * @param payload_len 数据段长度
 * @param out_frame   输出帧缓冲区
 * @param out_cap     输出缓冲区容量
 * @param out_len     输出实际帧长
 * @return true 成功；false 失败（长度超限/缓冲区不足/参数错误）
 */
bool PhoneReply_BuildFrameEx(uint16_t cmd,
							 uint8_t crypto,
							 uint8_t seq,
							 const uint8_t *payload,
							 uint16_t payload_len,
							 uint8_t *out_frame,
							 uint16_t out_cap,
							 uint16_t *out_len);

/**
 * @brief 0x0101：鉴权结果回复帧（Connect 0x01FE 的应答）
 * @note 这里负责“拼包”，发送动作由 protocol 层完成
 */
bool PhoneReply_BuildAuthResult_0101(uint8_t conidx,
									bool ok,
									uint8_t seq,
									uint8_t *out_frame,
									uint16_t out_cap,
									uint16_t *out_len);

/**
 * @brief 通用：仅包含 ResultCode(1) 的回复（很多“设置类”命令都可复用这种形态）
 * @param rsp_cmd     回复命令字（TODO：按协议文档填写）
 * @param result_code 0=成功，非0=失败（TODO：按协议文档映射）
 */
bool PhoneReply_BuildResultOnly(uint16_t rsp_cmd,
								uint8_t result_code,
								uint8_t seq,
								uint8_t *out_frame,
								uint16_t out_cap,
								uint16_t *out_len);

/**
 * @brief 通用：ResultCode(1) + Value(1) 的回复
 * @note 例如开关类查询/设置回读，或“音量”等单字节值
 */
bool PhoneReply_BuildResultU8(uint16_t rsp_cmd,
							  uint8_t result_code,
							  uint8_t value,
							  uint8_t seq,
							  uint8_t *out_frame,
							  uint16_t out_cap,
							  uint16_t *out_len);

/**
 * @brief 通用：ResultCode(1) + DataLen(1) + Data(N)
 * @note 用于返回可变长数据（如 MAC/NFC 列表等）。DataLen 1 字节上限 255。
 */
bool PhoneReply_BuildResultBlob(uint16_t rsp_cmd,
								uint8_t result_code,
								const uint8_t *data,
								uint8_t data_len,
								uint8_t seq,
								uint8_t *out_frame,
								uint16_t out_cap,
								uint16_t *out_len);

/* ===== 业务型回复包骨架（先不硬编码 cmd，避免没对齐文档就写死） ===== */

/**
 * @brief “解绑”类命令回复骨架
 * @note TODO：确认解绑回复的 cmd 与 payload 定义（多数情况可用 ResultOnly）
 */
bool PhoneReply_BuildUnpairReply(uint16_t rsp_cmd,
								 uint8_t result_code,
								 uint8_t seq,
								 uint8_t *out_frame,
								 uint16_t out_cap,
								 uint16_t *out_len);

/**
 * @brief “寻车”类命令回复骨架
 * @note TODO：确认是否需要回读寻车状态/音量等字段；否则可退化成 ResultOnly
 */
bool PhoneReply_BuildCarSearchReply(uint16_t rsp_cmd,
									uint8_t result_code,
									uint8_t car_search_volume,
									uint8_t seq,
									uint8_t *out_frame,
									uint16_t out_cap,
									uint16_t *out_len);

/**
 * @brief “获取手机 MAC”回复骨架
 * @note TODO：确认 payload 是否是 [MacLen+Mac] 或固定 6 字节；此处用 ResultBlob 占位
 */
bool PhoneReply_BuildGetPhoneMacReply(uint16_t rsp_cmd,
									  uint8_t result_code,
									  const uint8_t *mac,
									  uint8_t mac_len,
									  uint8_t seq,
									  uint8_t *out_frame,
									  uint16_t out_cap,
									  uint16_t *out_len);

/**
 * @brief “NFC 操作”回复骨架（添加/删除/查询）
 * @note TODO：确认 NFC 的返回格式（例如卡号/条目数/列表等）；此处先用 ResultBlob
 */
bool PhoneReply_BuildNfcOpReply(uint16_t rsp_cmd,
								uint8_t result_code,
								const uint8_t *nfc_data,
								uint8_t nfc_data_len,
								uint8_t seq,
								uint8_t *out_frame,
								uint16_t out_cap,
								uint16_t *out_len);

/*
 * ===== 下面是骨架：后续你把所有“需要给 app 回复的包”都按这个模式往下加 =====
 * 1) 先定义某个回复包的 payload 结构/字段（不一定用 struct，数组也行）
 * 2) 在 xxx_Build() 里填 payload
 * 3) 调 PhoneReply_BuildFrame() 组帧
 * 4) protocol/ble_function 只管调用 xxx_Build() + 发送
 */

#endif // PHONE_REPLY_H
