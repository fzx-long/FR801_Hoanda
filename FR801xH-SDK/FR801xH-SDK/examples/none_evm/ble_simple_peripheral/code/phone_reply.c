/**
 * @file phone_reply.c
 * @author Fanzx (1456925916@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-01-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "phone_reply.h"

#include "protocol.h"
#include "protocol_cmd.h"
#include "rssi_check.h"

#include <string.h>

/*
 * 0x0101 鉴权结果回复：联调阶段按你的要求“都用开的”。
 * - InductionStatus: 1=关, 2=开
 * - NfcSwitch:      1=关, 2=开
 * - CarSearchVolume: 0x01低 0x02中 0x03高 0x04关闭
 *
 * 为什么放这里：phone_reply 只负责拼包，业务侧只需要调用发送即可。
 */
#ifndef PHONE_REPLY_INDUCTION_STATUS
#define PHONE_REPLY_INDUCTION_STATUS   2u
#endif

#ifndef PHONE_REPLY_NFC_SWITCH
#define PHONE_REPLY_NFC_SWITCH         2u
#endif

#ifndef PHONE_REPLY_CAR_SEARCH_VOLUME
#define PHONE_REPLY_CAR_SEARCH_VOLUME  0x02u
#endif

static uint8_t phone_reply_xor_bcc(const uint8_t *buf, uint16_t len)
{
	uint8_t bcc = 0;
	if (buf == NULL || len == 0) {
		return 0;
	}
	for (uint16_t i = 0; i < len; i++) {
		bcc ^= buf[i];
	}
	return bcc;
}

bool PhoneReply_BuildFrame(uint16_t cmd,
						   uint8_t seq,
						   const uint8_t *payload,
						   uint16_t payload_len,
						   uint8_t *out_frame,
						   uint16_t out_cap,
						   uint16_t *out_len)
{
	/* 兼容旧接口：默认不加密 */
	return PhoneReply_BuildFrameEx(cmd,
							 CRYPTO_TYPE_NONE,
							 seq,
							 payload,
							 payload_len,
							 out_frame,
							 out_cap,
							 out_len);
}

bool PhoneReply_BuildFrameEx(uint16_t cmd,
							 uint8_t crypto,
							 uint8_t seq,
							 const uint8_t *payload,
							 uint16_t payload_len,
							 uint8_t *out_frame,
							 uint16_t out_cap,
							 uint16_t *out_len)
{
	if (out_frame == NULL || out_len == NULL) {
		return false;
	}

	const uint16_t total_len = (uint16_t)(PHONE_REPLY_FRAME_OVERHEAD + payload_len);
	if (total_len > 0xFFu) {
		return false; // 协议 length 字段 1 字节
	}
	if (out_cap < total_len) {
		return false;
	}

	out_frame[0] = 0x55;
	out_frame[1] = 0x55;
	out_frame[2] = (uint8_t)total_len;
	out_frame[3] = crypto;
	out_frame[4] = seq;
	/* Cmd：协议侧为大端序（例如 0x03FE 在帧里应为 03 FE） */
	out_frame[5] = (uint8_t)(cmd >> 8);
	out_frame[6] = (uint8_t)(cmd & 0xFF);

	if (payload_len > 0) {
		if (payload == NULL) {
			return false;
		}
		memcpy(&out_frame[7], payload, payload_len);
	}

	/* BCC：XOR，从 Header 到 Data（不包含 BCC/Footer） */
	const uint16_t bcc_len = (uint16_t)(7u + payload_len);
	out_frame[7 + payload_len] = phone_reply_xor_bcc(out_frame, bcc_len);

	out_frame[8 + payload_len] = 0xAA;
	out_frame[9 + payload_len] = 0xAA;

	*out_len = total_len;
	return true;
}

bool PhoneReply_BuildAuthResult_0101(uint8_t conidx,
									bool ok,
									uint8_t seq,
									uint8_t *out_frame,
									uint16_t out_cap,
									uint16_t *out_len)
{
	/* 0x0101 Data:
	 * ResultCode(1) + InductionStatus(1) + NfcSwitch(1) + CarSearchVolume(1) + ModelMacLen(1) + ModelMac(N)
	 * ResultCode: 0=成功，1=失败
	 */
	uint8_t peer_mac[PHONE_REPLY_MODEL_MAC_MAX_LEN];
	uint8_t mac_len = 0;
	if (RSSI_Check_Get_Peer_Addr(conidx, peer_mac)) {
		mac_len = PHONE_REPLY_MODEL_MAC_MAX_LEN;
	}

	uint8_t payload[5 + PHONE_REPLY_MODEL_MAC_MAX_LEN];
	payload[0] = ok ? 0 : 1;
	payload[1] = (uint8_t)PHONE_REPLY_INDUCTION_STATUS;
	payload[2] = (uint8_t)PHONE_REPLY_NFC_SWITCH;
	payload[3] = (uint8_t)PHONE_REPLY_CAR_SEARCH_VOLUME;
	payload[4] = mac_len;
	if (mac_len == PHONE_REPLY_MODEL_MAC_MAX_LEN) {
		memcpy(&payload[5], peer_mac, PHONE_REPLY_MODEL_MAC_MAX_LEN);
	}

	const uint16_t payload_len = (uint16_t)(5u + mac_len);
	return PhoneReply_BuildFrame(auth_result_ID, seq, payload, payload_len, out_frame, out_cap, out_len);
}

bool PhoneReply_BuildResultOnly(uint16_t rsp_cmd,
								uint8_t result_code,
								uint8_t seq,
								uint8_t *out_frame,
								uint16_t out_cap,
								uint16_t *out_len)
{
	uint8_t payload[1];
	payload[0] = result_code;
	return PhoneReply_BuildFrame(rsp_cmd, seq, payload, (uint16_t)sizeof(payload), out_frame, out_cap, out_len);
}

bool PhoneReply_BuildResultU8(uint16_t rsp_cmd,
							  uint8_t result_code,
							  uint8_t value,
							  uint8_t seq,
							  uint8_t *out_frame,
							  uint16_t out_cap,
							  uint16_t *out_len)
{
	uint8_t payload[2];
	payload[0] = result_code;
	payload[1] = value;
	return PhoneReply_BuildFrame(rsp_cmd, seq, payload, (uint16_t)sizeof(payload), out_frame, out_cap, out_len);
}

bool PhoneReply_BuildResultBlob(uint16_t rsp_cmd,
								uint8_t result_code,
								const uint8_t *data,
								uint8_t data_len,
								uint8_t seq,
								uint8_t *out_frame,
								uint16_t out_cap,
								uint16_t *out_len)
{
	/* payload: ResultCode(1) + DataLen(1) + Data(N) */
	uint8_t payload[2 + 255];
	payload[0] = result_code;
	payload[1] = data_len;
	if (data_len > 0) {
		if (data == NULL) {
			return false;
		}
		memcpy(&payload[2], data, data_len);
	}
	return PhoneReply_BuildFrame(rsp_cmd,
								 seq,
								 payload,
								 (uint16_t)(2u + (uint16_t)data_len),
								 out_frame,
								 out_cap,
								 out_len);
}

bool PhoneReply_BuildUnpairReply(uint16_t rsp_cmd,
								 uint8_t result_code,
								 uint8_t seq,
								 uint8_t *out_frame,
								 uint16_t out_cap,
								 uint16_t *out_len)
{
	/* TODO：对齐协议文档后，若解绑回复带额外字段，在这里扩展 payload */
	return PhoneReply_BuildResultOnly(rsp_cmd, result_code, seq, out_frame, out_cap, out_len);
}

bool PhoneReply_BuildCarSearchReply(uint16_t rsp_cmd,
									uint8_t result_code,
									uint8_t car_search_volume,
									uint8_t seq,
									uint8_t *out_frame,
									uint16_t out_cap,
									uint16_t *out_len)
{
	/* TODO：若协议要求更多字段（例如状态/持续时间），在这里扩展 */
	return PhoneReply_BuildResultU8(rsp_cmd, result_code, car_search_volume, seq, out_frame, out_cap, out_len);
}

bool PhoneReply_BuildGetPhoneMacReply(uint16_t rsp_cmd,
									  uint8_t result_code,
									  const uint8_t *mac,
									  uint8_t mac_len,
									  uint8_t seq,
									  uint8_t *out_frame,
									  uint16_t out_cap,
									  uint16_t *out_len)
{
	/* TODO：若协议规定固定 6 字节且无 Len 字段，则改成固定 payload */
	return PhoneReply_BuildResultBlob(rsp_cmd, result_code, mac, mac_len, seq, out_frame, out_cap, out_len);
}

bool PhoneReply_BuildNfcOpReply(uint16_t rsp_cmd,
								uint8_t result_code,
								const uint8_t *nfc_data,
								uint8_t nfc_data_len,
								uint8_t seq,
								uint8_t *out_frame,
								uint16_t out_cap,
								uint16_t *out_len)
{
	/* TODO：对齐 NFC 返回格式（可能是条目数+多条记录），在这里组装 data */
	return PhoneReply_BuildResultBlob(rsp_cmd, result_code, nfc_data, nfc_data_len, seq, out_frame, out_cap, out_len);
}
