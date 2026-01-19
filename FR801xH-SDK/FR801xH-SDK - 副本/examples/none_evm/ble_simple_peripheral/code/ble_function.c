/* TPMS 模拟数据 - 基于用户提供的格式 */
static const uint8_t s_tpms_sim_payload_0117[] = {
	0xFF, /* 轮位/通道：未知轮位 */
	0xE3, 0x17, 0x28, 0xCC, /* Sensor ID: 0xE31728CC */
	0x30, 0x54, /* 压力：0x3054 → 12372 → 123.72 kPa */
	0x00, /* 温度：0℃ */
	0x01, 0x2A, /* 电池电压：0x012A → 298 → 2.98 V */
	0x93, /* 状态：高4bit=0x9(Normal), 低4bit=0x3(Driving) */
	0xD4, 0xFE, 0x28, 0x35, 0x26, 0xEE /* MAC地址：D4:FE:28:35:26:EE */
};

static uint16_t BleFunc_MapBleCmdToMcuId(uint16_t ble_cmd, const uint8_t *payload, uint16_t payload_len)
{
	/*
	 * 关键点：BLE 下行的 cmd(如 0x0FFE/0x12FE) 不等于 MCU UART 的 id。
	 * MCU 侧命令号以 usart_cmd.h 里的 CMD_* 为准。
	 */
	switch (ble_cmd) {
		case 0x09FDu: /* 6.13 车辆防盗告警设置：MCU 侧复用油车设防命令 0x31 */
			(void)payload;
			(void)payload_len;
			return (uint16_t)CMD_OIL_VEHICLE_PREVENTION;
		case 0x02FDu: /* 助力推车模式 */
			if (payload_len >= 7u && payload != NULL) {
				uint8_t control = payload[6];
				if (control == 0x01u) return (uint16_t)CMD_Assistive_trolley_mode_on;
				if (control == 0x00u) return (uint16_t)CMD_Assistive_trolley_mode_off;
				if (control == 0x02u) return (uint16_t)CMD_Assistive_trolley_mode_default;
			}
			return 0u;
		case 0x03FEu: /* 5.3 设防/撤防 (复用油车设防指令) */
			if (payload_len >= 7u && payload != NULL) {
				return (payload[6] == 0x00u) ? (uint16_t)CMD_OIL_VEHICLE_UNPREVENTION : (uint16_t)CMD_OIL_VEHICLE_PREVENTION;
			}
			return 0u;
		case 0x03FDu: /* 延时大灯 */
			if (payload_len >= 7u && payload != NULL) {
				uint8_t control = payload[6];
				if (control == 0x01u) return (uint16_t)CMD_Delayed_headlight_on;
				if (control == 0x00u) return (uint16_t)CMD_Delayed_headlight_off;
				if (control == 0x02u) return (uint16_t)CMD_Delayed_headlight_default;
				if (control == 0x03u) return (uint16_t)CMD_Delayed_headlight_time_set;
			}
			return 0u;
		case 0x04FDu: /* 充电功率 */
			return (uint16_t)CMD_Charging_power_set;
		case 0x05FDu: /* 自动 P 档 */
			if (payload_len >= 7u && payload != NULL) {
				uint8_t control = payload[6];
				if (control == 0x01u) return (uint16_t)CMD_AUTO_P_GEAR_on;
				if (control == 0x00u) return (uint16_t)CMD_AUTO_P_GEAR_off;
				if (control == 0x02u) return (uint16_t)CMD_AUTO_P_GEAR_default;
				if (control == 0x03u) return (uint16_t)CMD_AUTO_P_GEAR_time_set;
			}
			return 0u;
		case 0x06FDu: /* 和弦喇叭：音源+音量（data 段由 payload 透传） */
			return (uint16_t)CMD_Chord_horn_type_set;
		case 0x07FDu: /* 三色氛围灯：模式设置 */
			return (uint16_t)CMD_Three_shooting_lamp_mode_set;
		case 0x08FDu: /* 辅助倒车档 */
			if (payload_len >= 7u && payload != NULL) {
				uint8_t control = payload[6];
				if (control == 0x01u) return (uint16_t)CMD_Assistive_reversing_gear_on;
				if (control == 0x00u) return (uint16_t)CMD_Assistive_reversing_gear_off;
				if (control == 0x02u) return (uint16_t)CMD_Assistive_reversing_gear_default;
			}
			return 0u;
		case 0x0AFDu: /* 自动转向回位 */
			if (payload_len >= 7u && payload != NULL) {
				uint8_t control = payload[6];
				if (control == 0x01u) return (uint16_t)CMD_Automatic_steering_reset_on;
				if (control == 0x00u) return (uint16_t)CMD_Automatic_steering_reset_off;
				if (control == 0x02u) return (uint16_t)CMD_Automatic_steering_reset_default;
			}
			return 0u;
		case 0x0BFEu: /* 添加 NFC 钥匙 */
			return (uint16_t)CMD_ADD_NFC_KEY;
		case 0x0CFEu: /* 删除 NFC 钥匙 */
			return (uint16_t)CMD_DELETE_NFC_KEY;
		case 0x0BFDu: /* EBS：开关/类型（data 段由 payload 透传） */
			return (uint16_t)CMD_EBS_switch_on;
		case 0x0CFDu: /* 低速档位 */
			if (payload_len >= 7u && payload != NULL) {
				uint8_t control = payload[6];
				if (control == 0x01u) return (uint16_t)CMD_Low_speed_gear_on;
				if (control == 0x00u) return (uint16_t)CMD_Low_speed_gear_off;
				if (control == 0x02u) return (uint16_t)CMD_Low_speed_gear_default;
				if (control == 0x03u) return (uint16_t)CMD_Low_speed_gear_speed_set;
			}
			return 0u;
		case 0x0DFDu: /* 中速档位 */
			if (payload_len >= 7u && payload != NULL) {
				uint8_t control = payload[6];
				if (control == 0x01u) return (uint16_t)CMD_Medium_speed_gear_on;
				if (control == 0x00u) return (uint16_t)CMD_Medium_speed_gear_off;
				if (control == 0x02u) return (uint16_t)CMD_Medium_speed_gear_default;
				if (control == 0x03u) return (uint16_t)CMD_Medium_speed_gear_speed_set;
			}
			return 0u;
	case 0x0EFDu: /* 高速档位 */
		if (payload_len >= 7u && payload != NULL) {
			uint8_t control = payload[6];
			if (control == 0x01u) return (uint16_t)CMD_High_speed_gear_on;
			if (control == 0x00u) return (uint16_t)CMD_High_speed_gear_off;
			if (control == 0x02u) return (uint16_t)CMD_High_speed_gear_default;
			if (control == 0x03u) return (uint16_t)CMD_High_speed_gear_speed_set;
		}
		return 0u;
	case 0x0CFDu: /* E-SAVE 模式设置 */
		if (payload_len >= 7u && payload != NULL) {
			uint8_t control = payload[6];
			if (control == 0x01u) return (uint16_t)CMD_Low_speed_gear_on;
			if (control == 0x00u) return (uint16_t)CMD_Low_speed_gear_off;
			if (control == 0x02u) return (uint16_t)CMD_Low_speed_gear_default;
			if (control == 0x03u) return (uint16_t)CMD_Low_speed_gear_speed_set;
		}
		return 0u;
	case 0x0DFDu: /* DYN 模式设置 */
		if (payload_len >= 7u && payload != NULL) {
			uint8_t control = payload[6];
			if (control == 0x01u) return (uint16_t)CMD_Medium_speed_gear_on;
			if (control == 0x00u) return (uint16_t)CMD_Medium_speed_gear_off;
			if (control == 0x02u) return (uint16_t)CMD_Medium_speed_gear_default;
			if (control == 0x03u) return (uint16_t)CMD_Medium_speed_gear_speed_set;
		}
		return 0u;
	case 0x0EFDu: /* SPORT 模式设置 */
		if (payload_len >= 7u && payload != NULL) {
			uint8_t control = payload[6];
			if (control == 0x01u) return (uint16_t)CMD_High_speed_gear_on;
			if (control == 0x00u) return (uint16_t)CMD_High_speed_gear_off;
			if (control == 0x02u) return (uint16_t)CMD_High_speed_gear_default;
			if (control == 0x03u) return (uint16_t)CMD_High_speed_gear_speed_set;
		}
		return 0u;
	case 0x0FFDu: /* 丢失模式 */
		if (payload_len >= 7u && payload != NULL) {
			return (payload[6] == 0x01u) ? (uint16_t)CMD_Lost_mode_on : (uint16_t)CMD_Lost_mode_off;
		}
		return 0u;
	case 0x10FDu: /* TCS */
		if (payload_len >= 7u && payload != NULL) {
			return (payload[6] == 0x01u) ? (uint16_t)CMD_TCS_switch_on : (uint16_t)CMD_TCS_switch_off;
		}
		return 0u;
	case 0x11FDu: /* 侧支架 */
		if (payload_len >= 7u && payload != NULL) {
			return (payload[6] == 0x01u) ? (uint16_t)CMD_Side_stand_switch_on : (uint16_t)CMD_Side_stand_switch_off;
		}
		return 0u;
	case 0x12FDu: /* 电池类型与容量 */
		if (payload_len >= 7u && payload != NULL) {
			uint8_t control = payload[6];
			if (control == 0x01u) return (uint16_t)CMD_Battery_type_set;
			if (control == 0x02u) return (uint16_t)CMD_Battery_capacity_set;
		}
		return 0u;
	case 0x0FFEu: /* 油车设防/解防 */
		if (payload_len >= 7u && payload != NULL) {
			return (payload[6] == 0x00u) ? (uint16_t)CMD_OIL_VEHICLE_UNPREVENTION : (uint16_t)CMD_OIL_VEHICLE_PREVENTION;
		}
		return 0u;
	case 0x08FEu: /* 5.8 手机蓝牙寻车 */
		/*
		 * MCU 命令号未单独定义"电车寻车"，目前沿用油车寻车状态查询命令。
		 * payload(含 Time6) 原样透传给 MCU，由 MCU 侧决定如何触发鸣笛/双闪等动作。
		 */
		return (uint16_t)CMD_OIL_VEHICLE_FIND_STATUS;
	case 0x11FEu: /* 油车寻车声音/音量（沿用 MCU 侧 0x32，data 带音量） */
		return (uint16_t)CMD_OIL_VEHICLE_FIND_STATUS;
	case 0x12FEu: /* 尾箱锁 */
		if (payload_len >= 7u && payload != NULL) {
			return (payload[6] == 0x00u) ? (uint16_t)CMD_OIL_VEHICLE_UNLOCK_TRUNK : (uint16_t)CMD_OIL_VEHICLE_LOCK_TRUNK;
		}
		return 0u;
	case 0x13FEu: /* NFC 开关 */
		return (uint16_t)CMD_NFC_SWITCH;
	case 0x14FEu: /* 座桶锁 */
		if (payload_len >= 7u && payload != NULL) {
			return (payload[6] == 0x00u) ? (uint16_t)CMD_VEHICLE_UNLOCK_SEAT : (uint16_t)CMD_VEHICLE_LOCK_SEAT;
		}
		return 0u;
	case 0x15FEu: /* 静音设置 */
		return (uint16_t)CMD_VEHICLE_MUTE_SETTING;
	case 0x16FEu: /* 中箱锁 */
		if (payload_len >= 7u && payload != NULL) {
			return (payload[6] == 0x00u) ? (uint16_t)CMD_VEHICLE_UNLOCK_MIDDLE_BOX : (uint16_t)CMD_VEHICLE_LOCK_MIDDLE_BOX;
		}
		return 0u;
	case 0x17FEu: /* 紧急模式 */
		if (payload_len >= 7u && payload != NULL) {
			return (payload[6] == 0x01u) ? (uint16_t)CMD_VEHICLE_EMERGENCY_MODE_LOCK : (uint16_t)CMD_VEHICLE_EMERGENCY_MODE_UNLOCK;
		}
		return 0u;
	case 0x18FEu: /* len==6 获取 MAC；len==7 单控开锁 */
		if (payload_len == 6u) {
			return (uint16_t)CMD_BLE_MAC_READ;
		}
		if (payload_len == 7u) {
			return (uint16_t)CMD_SINGLE_CONTROL_UNLOCK;
		}
		return 0u;
	case 0x19FDu: /* 和弦喇叭音量设置 */
		return (uint16_t)CMD_Chord_horn_volume_set;
	case 0x1AFDu: /* 雷达设置 */
		if (payload_len >= 7u && payload != NULL) {
			uint8_t control = payload[6];
			if (control == 0x01u) return (uint16_t)CMD_Radar_switch_on;
			if (control == 0x02u) return (uint16_t)CMD_Radar_switch_off;
			if (control == 0x03u) return (uint16_t)CMD_Radar_switch_default;
			if (control == 0x04u) return (uint16_t)CMD_Radar_sensitivity_set;
		}
		return 0u;
	case 0x1BFDu: /* 胎压监测 */
		return (uint16_t)CMD_Tire_pressure_monitoring_get;
	case 0x1CFDu: /* HDC/HHC 设置 */
		if (payload_len >= 7u && payload != NULL) {
			uint8_t control = payload[6];
			if (control == 0x01u) return (uint16_t)CMD_HDC_switch_on;
			if (control == 0x02u) return (uint16_t)CMD_HDC_switch_off;
			if (control == 0x03u) return (uint16_t)CMD_HHC_switch_on;
			if (control == 0x04u) return (uint16_t)CMD_HHC_switch_off;
		}
		return 0u;
	case 0x1DFDu: /* 起步强度设置 */
		return (uint16_t)CMD_Starting_strength_set;
	case 0x1EFDu: /* SPORT 模式设置 */
		if (payload_len >= 7u && payload != NULL) {
			uint8_t control = payload[6];
			if (control == 0x01u) return (uint16_t)CMD_SPORT_mode_on;
			if (control == 0x02u) return (uint16_t)CMD_SPORT_mode_off;
			if (control == 0x03u) return (uint16_t)CMD_SPORT_mode_default;
			if (control == 0x04u) return (uint16_t)CMD_SPORT_mode_type_set;
		}
		return 0u;
	case 0x1FFDu: /* ECO 模式设置 */
		if (payload_len >= 7u && payload != NULL) {
			uint8_t control = payload[6];
			if (control == 0x01u) return (uint16_t)CMD_ECO_mode_on;
			if (control == 0x02u) return (uint16_t)CMD_ECO_mode_off;
			if (control == 0x03u) return (uint16_t)CMD_ECO_mode_default;
			if (control == 0x04u) return (uint16_t)CMD_ECO_mode_type_set;
		}
		return 0u;
	case 0x20FDu: /* 车型数据同步 */
		return (uint16_t)CMD_KFA_2KGA_2MQA_data_sync;
	default:
		return 0u;
	}
}

static void BleFunc_McuTxn_Timeout(void *parg)
{
	(void)parg;
	if (!s_mcu_pending.active) {
		return;
	}

	co_printf("[MCU_TXN] timeout: conidx=%u req_feature=0x%04X resp_feature=0x%04X id=0x%04X reply=0x%04X\r\n",
			(unsigned)s_mcu_pending.conidx,
			(unsigned)s_mcu_pending.req_feature,
			(unsigned)s_mcu_pending.resp_feature,
			(unsigned)s_mcu_pending.id,
			(unsigned)s_mcu_pending.reply_cmd);

	BleFunc_SendResultToConidx(s_mcu_pending.conidx, s_mcu_pending.reply_cmd, 0x01);
	s_mcu_pending.active = false;
}

/**
 * @brief 开启一个与 MCU 的异步交互事务 (Transaction)
 * 
 * @details
 * 这是一个核心的"中间件"函数，用来处理【蓝牙请求 -> 转发 MCU -> 等待 MCU 回复 -> 回复蓝牙】的全流程。
 * 它的主要逻辑如下：
 * 1. 【防重入检查】：如果当前已经有一个指令在等待 MCU 回复（s_mcu_pending.active 且未超时），则拒绝新指令，直接回失败。
 *    这是为了防止串口阻塞或逻辑混乱，保证一问一答的顺序性。
 * 2. 【指令映射】：蓝牙协议中的 CMD ID (如 0x0BFE) 可能与 MCU 串口协议定义的 ID (如 0x20) 不同。
 *    这里会自动调用 BleFunc_MapBleCmdToMcuId 进行翻译。
 * 3. 【发送请求】：调用底层驱动 SocMcu_Frame_Send 将数据打包通过 UART 发送给 MCU。
 * 4. 【挂起等待】：
 *    - 将当前事务的状态（连接索引、期待的回复CMD、ID等）保存到 s_mcu_pending 全局结构体中。
 *    - 启动一个超时定时器 (BLEFUNC_MCU_TIMEOUT_MS)。
 *    - 函数立即返回（异步）。
 * 
 * 后续流程：
 * - 成功路径：MCU 在超时前回复串口数据 -> 触发 BleFunc_OnMcuUartFrame -> 匹配 ID -> 取消定时器 -> 回复 APP 成功数据。
 * - 超时路径：定时器到期 -> 触发 BleFunc_McuTxn_Timeout -> 清除事务状态 -> 回复 APP 失败 (0x01)。
 * 
 * @param conidx      发起请求的蓝牙连接句柄（用于知道是谁发起的，将来回包给谁）
 * @param reply_cmd   将来回复给 APP 时使用的 BLE CMD ID（例如收到 0x0BFE，它是 0x0B01）
 * @param feature     通道号（通常是 SOC_MCU_FEATURE_FF01）
 * @param id          BLE 指令原始 ID (如 0x0BFE，将在此函数内尝试被翻译成 MCU ID)
 * @param payload     透传的数据内容
 * @param payload_len 数据长度
 */
static void BleFunc_McuTxn_Start(uint8_t conidx,
					uint16_t reply_cmd,
					uint16_t feature,
					uint16_t id,
					const uint8_t *payload,
					uint16_t payload_len)
{
	/* 1. 忙检测：确保上一个指令已经结束，避免串口并发冲突 */
	if (s_mcu_pending.active) {
		co_printf("[MCU_TXN] busy, reject new cmd\r\n");
		BleFunc_SendResultToConidx(conidx, reply_cmd, 0x01);
		return;
	}

	if (!s_mcu_pending.timer_inited) {
		os_timer_init(&s_mcu_pending.timer, BleFunc_McuTxn_Timeout, NULL);
		s_mcu_pending.timer_inited = true;
	}

	/* 2. ID 映射：将 BLE 协议 ID (如 0x..FE) 转换为 MCU 串口协议 ID (usart_cmd.h 中的 define) */
	uint16_t mcu_id = id;
	/* 通常 BLE 侧以 FE/FD 结尾的命令号不是 MCU 实际的指令号，需要翻译 */
	if (((id & 0x00FFu) == 0x00FEu) || ((id & 0x00FFu) == 0x00FDu)) {
		mcu_id = BleFunc_MapBleCmdToMcuId(id, payload, payload_len);
		if (mcu_id == 0u) {
			co_printf("[MCU_TXN] unknown BLE cmd to MCU id map: 0x%04X\r\n", (unsigned)id);
			/* 映射失败（未定义的指令），直接回失败 */
			BleFunc_SendResultToConidx(conidx, reply_cmd, 0x01);
			return;
		}
	}

	/* 3. 发送串口数据给 MCU */
	uint8_t ok = SocMcu_Frame_Send(SOC_MCU_SYNC_SOC_TO_MCU, feature, mcu_id, payload, payload_len);
	if (!ok) {
		co_printf("[MCU_TXN] uart send failed\r\n");
		BleFunc_SendResultToConidx(conidx, reply_cmd, 0x01);
		return;
	}

	/* 4. 记录事务上下文并启动超时计时器 */
	s_mcu_pending.active = true;
	s_mcu_pending.conidx = conidx;
	s_mcu_pending.reply_cmd = reply_cmd;
	/*
	 * feature：发送请求时使用的通道（通常 FF01）
	 * resp_feature：匹配回包时使用的通道（通常 FF02）；同时兼容少量 MCU 固件会回同 feature 的情况。
	 */
	s_mcu_pending.req_feature = feature;
	s_mcu_pending.resp_feature = BLEFUNC_MCU_FEATURE_RESP;
	s_mcu_pending.id = mcu_id; /* 注意：这里记录的是发给 MCU 的 ID，用于匹配回包 */
	os_timer_start(&s_mcu_pending.timer, BLEFUNC_MCU_TIMEOUT_MS, false);
}

/**
 * @brief [DEBUG-SIM 专用] 仅通过串口下发给 MCU，不挂起等待、不影响当前 BLE 的模拟回包。
 *
 * @why
 * - 你要求在 ENABLE_NFC_ADD_SIMULATION 的 #else 分支里"保留原有模拟成功逻辑"，
 *   但同时"也要把指令直接用串口接口发给 MCU"。
 * - 生产链路仍建议使用 BleFunc_McuTxn_Start()（带超时/匹配/回包）。
 */
static void BleFunc_McuUart_SendOnly(uint16_t feature,
						uint16_t ble_cmd,
						const uint8_t *payload,
						uint16_t payload_len)
{
	uint16_t mcu_id = ble_cmd;
	if (((ble_cmd & 0x00FFu) == 0x00FEu) || ((ble_cmd & 0x00FFu) == 0x00FDu)) {
		mcu_id = BleFunc_MapBleCmdToMcuId(ble_cmd, payload, payload_len);
		if (mcu_id == 0u) {
			co_printf("[DEBUG_SIM] uart skip: map ble_cmd=0x%04X failed\r\n", (unsigned)ble_cmd);
			return;
		}
	}

	uint8_t ok = SocMcu_Frame_Send(SOC_MCU_SYNC_SOC_TO_MCU, feature, mcu_id, payload, payload_len);
	co_printf("[DEBUG_SIM] uart send: ble_cmd=0x%04X -> mcu_id=0x%04X ok=%u\r\n",
			(unsigned)ble_cmd,
			(unsigned)mcu_id,
			(unsigned)ok);
}

static bool BleFunc_PrintTime6(const uint8_t *time6)
{
	char time_str[13];
	if (proto_time6_bcd_to_str12(time6, time_str)) {
		co_printf("    time_bcd=%s\r\n", time_str);
		return true;
	}
	co_printf("    time_bcd=<invalid bcd>\r\n");
	return false;
}
/*********************************************************************
 * @brief 进行回复给蓝牙app
 * @param reply_cmd {placeholder}
 * @param result_code {placeholder}
 */
static void BleFunc_SendResultToRx(uint16_t reply_cmd, uint8_t result_code)
{
	uint8_t conidx = Protocol_Get_Rx_Conidx();
	uint8_t resp_payload[1];
	resp_payload[0] = result_code;
	(void)Protocol_Send_Unicast(conidx, reply_cmd, resp_payload, (uint16_t)sizeof(resp_payload));
}

/*********************************************************************
 * @brief 发送默认回复（通常用于未实现的功能或默认失败情况）
 * @param reply_cmd 回复的命令ID
 */
#if defined(__CC_ARM)
#pragma diag_suppress 177 /* function was declared but never referenced */
#endif
static void BleFunc_SendResultToRx_Default(uint16_t reply_cmd)
{
	/* 默认回失败 (0x01) */
	BleFunc_SendResultToRx(reply_cmd, 0x01);
}


static void BleFunc_SendResultToRx_WithData(uint16_t reply_cmd, const uint8_t *data, uint16_t len)
{
	uint8_t conidx = Protocol_Get_Rx_Conidx();
	(void)Protocol_Send_Unicast(conidx, reply_cmd, data, len);
}
/*********************************************************************
 * @brief 确保当前连接已鉴权
 * @param reply_cmd {placeholder}
 * @retval {placeholder}  
 * @retval {placeholder}  
 */
static bool BleFunc_EnsureAuthed(uint16_t reply_cmd)
{
	uint8_t conidx = Protocol_Get_Rx_Conidx();
	if (Protocol_Auth_IsOk(conidx)) {
		return true;
	}
	co_printf("    auth not ok, reject cmd\r\n");
	BleFunc_SendResultToRx_Default(reply_cmd);
	return false;
}
/*********************************************************************
 * @brief 打印 Payload 内容（十六进制）
 * @param payload {placeholder}
 * @param len {placeholder}
 */
static void BleFunc_DumpPayload(const uint8_t *payload, uint8_t len)
{
	if (payload == NULL || len == 0) {
		co_printf("    payload: <empty>\r\n");
		return;
	}
	co_printf("    payload(%d):", (int)len);
	for (uint8_t i = 0; i < len; i++) {
		co_printf(" %02X", payload[i]);
	}
	co_printf("\r\n");
}

/* ====== 5.1 Connect(0x01FE) 辅助函数 ======
 * 说明：这些是"工具函数"，放在文件级以满足标准 C（禁止函数嵌套定义）。
 */
static uint8_t BleFunc_ToUpper(uint8_t c)
{
	if (c >= 'a' && c <= 'z') {
		return (uint8_t)(c - 'a' + 'A');
	}
	return c;
}

static bool BleFunc_IsHexAscii(uint8_t c)
{
	c = BleFunc_ToUpper(c);
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

static void BleFunc_DumpHexInline(const uint8_t *data, uint16_t dump_len)
{
	if (data == NULL || dump_len == 0) {
		co_printf("<empty>");
		return;
	}
	for (uint16_t i = 0; i < dump_len; i++) {
		co_printf("%02X", data[i]);
	}
}

static bool BleFunc_IsAllZero(const uint8_t *data, uint16_t len)
{
	if (data == NULL || len == 0u) {
		return true;
	}
	for (uint16_t i = 0; i < len; i++) {
		if (data[i] != 0u) {
			return false;
		}
	}
	return true;
}

void BleFunc_OnMcuUartFrame(uint16_t sync,
					uint16_t feature,
					uint16_t id,
					const uint8_t *data,
					uint16_t data_len,
					uint8_t crc_ok)
{
	if (!crc_ok) {
		co_printf("[MCU_TXN] drop frame: crc bad\r\n");
		return;
	}
	if (sync != SOC_MCU_SYNC_MCU_TO_SOC) {
		return;
	}

	/* 1) 如果是正在等待的 MCU 应答，则按事务回包到 APP（reply_cmd） */
	if (s_mcu_pending.active
			&& id == s_mcu_pending.id
			&& (feature == s_mcu_pending.resp_feature || feature == s_mcu_pending.req_feature)) {
		os_timer_stop(&s_mcu_pending.timer);
		s_mcu_pending.active = false;

		co_printf("[MCU_TXN] matched: conidx=%u feature=0x%04X id=0x%04X reply=0x%04X data_len=%u\r\n",
				(unsigned)s_mcu_pending.conidx,
				(unsigned)feature,
				(unsigned)id,
				(unsigned)s_mcu_pending.reply_cmd,
				(unsigned)data_len);

		if (data_len == 0u || data == NULL) {
			BleFunc_SendResultToConidx(s_mcu_pending.conidx, s_mcu_pending.reply_cmd, 0x00);
			return;
		}

		/* 约定：MCU payload = ResultCode(1) + 可选数据；BLE 侧原样透传 */
		(void)Protocol_Send_Unicast(s_mcu_pending.conidx, s_mcu_pending.reply_cmd, data, data_len);
		return;
	}

	/* 2) 非事务类回包：认为是 MCU 主动上报，转发到已鉴权 APP（Notify） */
	/* TPMS 0x0117：若 MCU 现阶段回全 0，则用模拟数据兜底，先把 App 链路跑通 */
	if (BLEFUNC_TPMS_SIM_ENABLE
			&& feature == (uint16_t)BLEFUNC_MCU_FEATURE_RESP
			&& id == (uint16_t)CMD_Tire_pressure_monitoring_get
			&& data_len == (uint16_t)sizeof(s_tpms_sim_payload_0117)
			&& BleFunc_IsAllZero(data, data_len)) {
		co_printf("[TPMS_SIM] replace all-zero payload: id=0x%04X len=%u\r\n",
				(unsigned)id,
				(unsigned)data_len);
		BleFunc_PushMcuFrameToAuthedApp(feature, id, s_tpms_sim_payload_0117, (uint16_t)sizeof(s_tpms_sim_payload_0117));
		return;
	}

	BleFunc_PushMcuFrameToAuthedApp(feature, id, data, data_len);
}
/*********************************************************************
 * @brief  5.1 连接指令（APP -> 设备，鉴权登录）
 * @param cmd {placeholder}
 * @param payload {placeholder}
 * @param len {placeholder}
 * @note 该指令用于 APP 端登录鉴权，payload 包含时间戳、Token 等信息。
 */
void BleFunc_FE_Connect(uint16_t cmd, const uint8_t* payload, uint8_t len)
{
	(void)cmd;
	co_printf("  -> Connect\r\n");

	/*
	 * 文档定义：Time(6) + Token(32=MD5(T-BOXKEY)的HEX字符串) + mobileSystem(1)
	 * 兼容性保留：若对端发送 16 字节原始 MD5，则按 Time(6)+Token(16)+mobileSystem(1) 解析。
	 */

	// 0x01FE 数据段定义：Time(6) + Token(32) + mobileSystem(1) = 39
	// 兼容性修改：如果长度 > 39，可能是因为 AES 加密填充（Padding），只取前 39 字节
	if (payload == NULL || len < 39) {
		co_printf("    invalid payload len=%d (expect >= 39)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
        // 校验失败，断开
        Protocol_Disconnect(Protocol_Get_Rx_Conidx());
		return;
	}

	const uint8_t *time6 = &payload[0];
	const uint8_t *token_ptr = &payload[6];
	uint8_t token_len = 0;
	uint8_t mobileSystem = 0xFF;

	bool token32_is_ascii_hex = true;
	for (uint8_t i = 0; i < 32; i++) {
		if (!BleFunc_IsHexAscii(token_ptr[i])) {
			token32_is_ascii_hex = false;
			break;
		}
	}

	/* 优先按"32字节ASCII HEX"解析；否则回退为"16字节原始MD5"解析 */
	if (token32_is_ascii_hex) {
		token_len = 32;
		mobileSystem = payload[38];
	} else {
		/* 16B Token: Time(6) + Token(16) + mobileSystem(1) = 23 */
		token_len = 16;
		mobileSystem = payload[22];
	}

	/* mobileSystem: 1=android, 2=ios, 3=other（按文档） */
	co_printf("    mobileSystem=0x%02X\r\n", mobileSystem);
	if (!(mobileSystem == 1 || mobileSystem == 2 || mobileSystem == 3)) {
		co_printf("    mobileSystem invalid (expect 1/2/3)\r\n");
	}

	/* Token 打印：若是 ASCII HEX，则按字符串更直观；否则按 HEX 输出 */
	if (token_len == 32 && token32_is_ascii_hex) {
		char rx_token_str[33];
		memcpy(rx_token_str, token_ptr, 32);
		rx_token_str[32] = '\0';
		co_printf("    Rx Token(32 str): %s\r\n", rx_token_str);
	} else {
		co_printf("    Rx Token(%d hex): ", token_len);
		BleFunc_DumpHexInline(token_ptr, token_len);
		co_printf("\r\n");
	}

	char time_str[13];
	if (proto_time6_bcd_to_str12(time6, time_str)) {
		co_printf("    time_bcd=%s\r\n", time_str);
	} else {
        co_printf("    time_bcd=invalid\r\n");
		/* [Debug] 解析失败时，打印前 16 个字节，检查解密后的数据 */
        co_printf("    Decrypted Hex(16): ");
        for(int i=0; i<16 && i<len; i++) co_printf("%02X ", payload[i]);
        co_printf("\r\n");
    }

	/* --- MD5 Token Verification --- */
	uint8_t target_digest[16];
	Algo_MD5_Calc(TBOX_KEY, sizeof(TBOX_KEY), target_digest);

	// Convert Digest to Hex String (upper case)
	char calculated_token[33];
	const char hex_map[] = "0123456789ABCDEF";
	for(int i=0; i<16; i++) {
		calculated_token[i*2]     = hex_map[(target_digest[i] >> 4) & 0x0F];
		calculated_token[i*2 + 1] = hex_map[target_digest[i] & 0x0F];
	}
	calculated_token[32] = '\0'; // Null terminator

	/* 默认期望 Token：用设备侧 TBOX_KEY 计算出来的 MD5 HEX */
	char expected_token_up[33];
	memcpy(expected_token_up, calculated_token, 33);

	/* 如果配置了覆盖宏，则以覆盖值为准（忽略大小写） */
	if (sizeof(TBOX_TOKEN_MD5_ASCII) > 1u) {
		for (uint8_t i = 0; i < 32; i++) {
			expected_token_up[i] = (char)BleFunc_ToUpper((uint8_t)TBOX_TOKEN_MD5_ASCII[i]);
		}
		expected_token_up[32] = '\0';
		co_printf("    Local Expected Token(override): %s\r\n", expected_token_up);
	} else {
		co_printf("    Local Calc Token: %s\r\n", expected_token_up);
	}

	/* Compare received token with calculated token (兼容两种格式) */
	bool auth_ok = false;
#if BLEFUNC_DEV_ACCEPT_ANY_TOKEN
	/* 开发阶段：只要 0x01FE 解析成功，就认为鉴权通过（不比较 Token，不断连） */
	auth_ok = true;
#else
	if (token_len == 32) {
		/* 32 字节 ASCII HEX：忽略大小写 */
		uint8_t rx_up[32];
		for (uint8_t i = 0; i < 32; i++) rx_up[i] = BleFunc_ToUpper(token_ptr[i]);
		if (memcmp(rx_up, expected_token_up, 32) == 0) {
			auth_ok = true;
		}
	} else if (token_len == 16) {
		/* 16 字节原始 MD5 */
		if (memcmp(token_ptr, target_digest, 16) == 0) {
			auth_ok = true;
		}
	}
#endif

	if (auth_ok) {
		co_printf("    Auth Success!\r\n");
		uint8_t conidx = Protocol_Get_Rx_Conidx();
		Protocol_Auth_Set(conidx, true);
		/*回传鉴权成功*/
		Protocol_Auth_SendResult(conidx, true);
		/*
		 * 6.10/6.11：鉴权成功后主动同步一次参数到 APP。
		 * 为什么放在这里：
		 * - 文档要求"每次蓝牙连接上后同步一次"，而工程里业务推送只对已鉴权连接开放。
		 * - 放在鉴权成功点，能避免未登录连接收到业务帧导致 APP 误解析。
		 */
		ParamSync_OnBleAuthed(conidx);
	} else { 
		co_printf("    Auth Failed!\r\n");
		
		uint8_t conidx = Protocol_Get_Rx_Conidx();
		Protocol_Auth_Set(conidx, false);
		Protocol_Auth_SendResult(conidx, false);
		
		// 断开连接
		Protocol_Disconnect(conidx);
	}
}

/*********************************************************************
 * @brief 5.3 设防/撤防设置指令（APP -> 设备，开关电门锁）
 * @details
 * - Request Cmd: 0x03FE
 * - Payload: Time(6) + DefendStatus(1)
 *   - DefendStatus: 0x00 手动撤防；0x01 手动设防
 * - Response Cmd: 0x0301
 * - Response Payload: ResultCode(1)
 *   - 0x00 成功；0x01 失败
 * @note
 * - 该指令通常会涉及 IO/继电器动作（电门锁）。
 * - 当前实现做协议解析与回包框架；硬件动作未接入时默认回失败，避免误控。
 */
void BleFunc_FE_Defences(uint16_t cmd, const uint8_t* payload, uint8_t len)
{
	co_printf("  -> Defences (0x%04X)\r\n", cmd);
	uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd); // 0x0301

	if (!BleFunc_EnsureAuthed(reply_cmd)) return;

	/* 0x03FE: Time(6) + DefendStatus(1) = 7 bytes */
	if (payload == NULL || len < 7) {
		co_printf("    invalid payload len=%d (expect >= 7)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
		BleFunc_SendResultToRx(reply_cmd, 0x01);
		return;
	}

	const uint8_t *time6 = &payload[0];
	uint8_t defendStatus = payload[6];

	BleFunc_PrintTime6(time6);
	co_printf("    DefendStatus=0x%02X\r\n", defendStatus);
	BleFunc_DumpPayload(payload, len);

	uint8_t result = 0x01;
	/* 校验 DefendStatus 合法性 */
	if (defendStatus != 0x00 && defendStatus != 0x01) {
		co_printf("    invalid DefendStatus\r\n");
		result = 0x01;
		BleFunc_SendResultToRx(reply_cmd, result);
	} else {
	#if (!ENABLE_NFC_ADD_SIMULATION)
		/* [PRODUCTION Mode] 真实转发给 MCU */
		{
			uint8_t conidx = Protocol_Get_Rx_Conidx();
			/* MapBleCmdToMcuId 将 0x03FE 转换为 0x30(撤防) / 0x31(设防) */
			BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, (uint16_t)cmd, payload, (uint16_t)len);
		}
	#else
		/* [DEBUG-ONLY] 模拟 MCU 回复成功 */
		BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
		co_printf("[DEBUG_SIM] Defences(%s) Success (0x00)\r\n", defendStatus?"LOCK":"UNLOCK");
		result = 0x00;
		BleFunc_SendResultToRx(reply_cmd, result);
	#endif
	}
}

/**
 * @brief 5.4 蓝牙感应防盗开关设置（APP -> 设备）
 * @details
 * - Request Cmd: 0x04FE
 * - Payload: Time(6) + InductionStatus(1)
 *   - 0x00 感应撤防；0x01 感应设防
 * - Response Cmd: 0x0401
 * - Response Payload: ResultCode(1)（0x00 成功；0x01 失败）
 */
void BleFunc_FE_AntiTheft(uint16_t cmd, const uint8_t* payload, uint8_t len)
{
	co_printf("  -> Anti-Theft (0x%04X)\r\n", cmd);
	uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
	if (!BleFunc_EnsureAuthed(reply_cmd)) return;

	/* 0x04FE: Time(6) + InductionStatus(1) = 7 bytes */
	if (payload == NULL || len < 7) {
		co_printf("    invalid payload len=%d (expect >= 7)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
		BleFunc_SendResultToRx(reply_cmd, 0x01);
		return;
	}

	BleFunc_PrintTime6(&payload[0]);
	co_printf("    InductionStatus=0x%02X\r\n", payload[6]);
	BleFunc_DumpPayload(payload, len);

	/* 下发到 MCU，并等待回包/超时再回 0x0401 */
	#if (!ENABLE_NFC_ADD_SIMULATION)
		/* [PRODUCTION Mode] 真实转发给 MCU */
		{
			uint8_t conidx = Protocol_Get_Rx_Conidx();
			BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, (uint16_t)cmd, payload, (uint16_t)len);
		}
	#else
		/* [DEBUG-ONLY] 模拟 MCU 回复成功（同时镜像下发 MCU 便于联调对照） */
		BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
		co_printf("[DEBUG_SIM] Anti-Theft Success (0x00)\r\n");
		BleFunc_SendResultToRx(reply_cmd, 0x00);
	#endif
}	

/**
 * @brief 5.5 手机消息通知（APP -> 设备）
 * @details
 * - Request Cmd: 0x05FE
 * - Payload: Time(6) + MessageType(1)
 *   - 0x01 有电话待接听；0x02 有信息待查看
 * - Response Cmd: 0x0501
 * - Response Payload: ResultCode(1)
 * @note 该指令通常不涉及危险硬件动作，默认回成功以保证 APP 流程联调。
 */
void BleFunc_FE_PhoneMessage(uint16_t cmd, const uint8_t* payload, uint8_t len)
{
	co_printf("  -> Phone Message (0x%04X)\r\n", cmd);
	uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
	if (!BleFunc_EnsureAuthed(reply_cmd)) return;

	/* 0x05FE: Time(6) + MessageType(1) = 7 bytes */
	if (payload == NULL || len < 7) {
		co_printf("    invalid payload len=%d (expect >= 7)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
		BleFunc_SendResultToRx(reply_cmd, 0x01);
		return;
	}

	BleFunc_PrintTime6(&payload[0]);
	co_printf("    MessageType=0x%02X\r\n", payload[6]);
	BleFunc_DumpPayload(payload, len);

	/* 下发到 MCU，并等待回包/超时再回 0x0501 */
	#if (!ENABLE_NFC_ADD_SIMULATION)
		/* [PRODUCTION Mode] 真实转发给 MCU */
		{
			uint8_t conidx = Protocol_Get_Rx_Conidx();
			BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, (uint16_t)cmd, payload, (uint16_t)len);
		}
	#else
		/* [DEBUG-ONLY] 模拟 MCU 回复成功（同时镜像下发 MCU 便于联调对照） */
		BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
		co_printf("[DEBUG_SIM] Phone Message Success (0x00)\r\n");
		BleFunc_SendResultToRx(reply_cmd, 0x00);
	#endif
}

/**
 * @brief 5.7 手机蓝牙信号强度范围设置（APP -> 设备）
 * @details
 * - Request Cmd: 0x07FE
 * - Payload: Time(6) + min(2) + max(2)
 *   - 协议说明：因子 -1，MCU 收到后 min/max 乘 -1
 * - Response Cmd: 0x0701
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FE_RssiStrength(uint16_t cmd, const uint8_t* payload, uint8_t len)
{
	co_printf("  -> RSSI Strength (0x%04X)\r\n", cmd);
	uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
	if (!BleFunc_EnsureAuthed(reply_cmd)) return;

	/* 0x07FE: Time(6) + min(2) + max(2) = 10 bytes */
	if (payload == NULL || len < 10) {
		co_printf("    invalid payload len=%d (expect >= 10)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
		BleFunc_SendResultToRx(reply_cmd, 0x01);
		return;
	}

	BleFunc_PrintTime6(&payload[0]);
	/* 解析 min_raw 和 max_raw 的值 */
	uint16_t min_raw = (uint16_t)payload[6] | ((uint16_t)payload[7] << 8);
	uint16_t max_raw = (uint16_t)payload[8] | ((uint16_t)payload[9] << 8);
	int16_t min_dbm = (int16_t)(-(int16_t)min_raw);// 转换成为正数
	int16_t max_dbm = (int16_t)(-(int16_t)max_raw);// 转换成为正数
	co_printf("    min_raw=%u -> %d\r\n", (unsigned)min_raw, (int)min_dbm);
	co_printf("    max_raw=%u -> %d\r\n", (unsigned)max_raw, (int)max_dbm);

	BleFunc_DumpPayload(payload, len);

	/* 下发到 MCU，并等待回包/超时再回 0x0701 */
	#if ((!BLEFUNC_FE_AUTO_REPLY_ENABLE) || (!BLEFUNC_FE_AUTO_REPLY_RSSI_STRENGTH))
		/* [PRODUCTION Mode] 真实转发给 MCU */
		{
			uint8_t conidx = Protocol_Get_Rx_Conidx();
			BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, (uint16_t)cmd, payload, (uint16_t)len);
		}
	#else
		/* [DEBUG-ONLY] 自动成功回包（同时镜像下发 MCU 便于联调对照） */
		BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
		co_printf("[DEBUG_SIM] RSSI Strength Success (0x00)\r\n");
		BleFunc_SendResultToRx(reply_cmd, 0x00);
	#endif
}

/**
 * @brief 5.8 手机蓝牙寻车（APP -> 设备）
 * @details
 * - Request Cmd: 0x08FE
 * - Payload: Time(6)
 * - Response Cmd: 0x0801
 * - Response Payload: ResultCode(1)
 * @note 通常会触发鸣笛/双闪等动作，未接入硬件前默认回失败。
 */
void BleFunc_FE_CarSearch(uint16_t cmd, const uint8_t* payload, uint8_t len)
{
	co_printf("  -> Car Search (0x%04X)\r\n", cmd);
	uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
	if (!BleFunc_EnsureAuthed(reply_cmd)) return;

	/* 0x08FE: Time(6) = 6 bytes */
	if (payload == NULL || len < 6) {
		co_printf("    invalid payload len=%d (expect >= 6)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
		BleFunc_SendResultToRx(reply_cmd, 0x01);
		return;
	}

	BleFunc_PrintTime6(&payload[0]);
	BleFunc_DumpPayload(payload, len);
	
	#if (!ENABLE_NFC_ADD_SIMULATION)
		/* [PRODUCTION Mode] 真实转发给 MCU，并等待 MCU 回包/超时后再回 0x0801 */
		{
			uint8_t conidx = Protocol_Get_Rx_Conidx();
			BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, (uint16_t)cmd, payload, (uint16_t)len);
		}
	#else
		/* [DEBUG-ONLY] 模拟 MCU 回复成功 */
		BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
		co_printf("[DEBUG_SIM] Car Search Success (0x00)\r\n");
		BleFunc_SendResultToRx(reply_cmd, 0x00);
	#endif
}

/**
 * @brief 5.9 设备蓝牙恢复出厂设置（APP -> 设备）
 * @details
 * - Request Cmd: 0x09FE
 * - Payload: Time(6)
 * - Response Cmd: 0x0901
 * - Response Payload: ResultCode(1)
 * @note 属于破坏性操作，未接入持久化/清理流程前默认回失败。soc端不用做处理，让vcu端进行处理即可。
 */
void BleFunc_FE_FactorySettings(uint16_t cmd, const uint8_t* payload, uint8_t len)
{
	co_printf("  -> Factory Settings (0x%04X)\r\n", cmd);
	uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
	if (!BleFunc_EnsureAuthed(reply_cmd)) return;

	/* 0x09FE: Time(6) = 6 bytes */
	if (payload == NULL || len < 6) {
		co_printf("    invalid payload len=%d (expect >= 6)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
		BleFunc_SendResultToRx(reply_cmd, 0x01);
		return;
	}
	BleFunc_PrintTime6(&payload[0]);
	BleFunc_DumpPayload(payload, len);

	/* TODO: 清理配对/参数/NFC 卡池等，并持久化 */
	#if (!ENABLE_NFC_ADD_SIMULATION)
		/* [PRODUCTION Mode] 转发 MCU 进行整车复位 */
		{
			uint8_t conidx = Protocol_Get_Rx_Conidx();
			BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, (uint16_t)cmd, payload, (uint16_t)len);
		}
	#else
		/* [DEBUG-ONLY] 模拟 MCU 回复成功 */
		BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
		co_printf("[DEBUG_SIM] Factory Settings Success (0x00)\r\n");
		BleFunc_SendResultToRx(reply_cmd, 0x00);
	#endif
}

/**
 * @brief 5.10 取消手机当前配对信息（APP -> 设备）
 * @details
 * - Request Cmd: 0x0AFE
 * - Payload: Time(6)
 * - Response Cmd: 0x0A01
 * - Response Payload: ResultCode(1)
 *   - 0x00 成功；0x01 失败；0x02 操作失败：无效卡（协议文档给出的扩展含义）
 */
void BleFunc_FE_BleUnpair(uint16_t cmd, const uint8_t* payload, uint8_t len)
{
	co_printf("  -> BLE Unpair (0x%04X)\r\n", cmd);
	uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
	if (!BleFunc_EnsureAuthed(reply_cmd)) return;

	/* 0x0AFE: Time(6) = 6 bytes */
	if (payload == NULL || len < 6) {
		co_printf("    invalid payload len=%d (expect >= 6)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
		BleFunc_SendResultToRx(reply_cmd, 0x01);
		return;
	}
	BleFunc_PrintTime6(&payload[0]);
	BleFunc_DumpPayload(payload, len);

	/*
	 * 取消绑定/配对信息：
	 * - SDK bond manager 会把密钥/对端信息持久化到 flash（初始化见 ble_simple_peripheral.c 里 gap_bond_manager_init）
	 * - 此处调用 gap_bond_manager_delete_all() 清空所有 bond 记录
	 * - 先回包再断链，避免在断链后无法通过 notify 回 0x0A01
	 */
	BleFunc_SendResultToRx(reply_cmd, 0x00);

	uint8_t conidx = Protocol_Get_Rx_Conidx();
	Protocol_Auth_Clear(conidx);
	gap_bond_manager_delete_all();
	Protocol_Disconnect(conidx);
}

/**
 * @brief 5.11 手机增实体 NFC 卡（APP -> 设备）
 * @details
 * - Request Cmd: 0x0BFE
 * - Payload: Time(6) + add_control(1)
 *   - 0x01 开始添加；0x02 确定添加非加密卡；0x03 不添加非加密卡
 * - Response Cmd: 0x0B01
 * - Response Payload: ResultCode(1) + (uuid/key 可选)
 * @note
 * - 该功能涉及 NFC 识别/加密/卡池存储/超时流程。
 * - 当前实现仅解析与回包框架；未接入卡池时默认回失败。
 */
void BleFunc_FE_AddNfc(uint16_t cmd, const uint8_t* payload, uint8_t len)
{
	co_printf("  -> Add NFC (0x%04X)\r\n", cmd);
	uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
	if (!BleFunc_EnsureAuthed(reply_cmd)) return;

	if (payload == NULL || len != 7) {
		co_printf("    invalid payload len=%d (expect 7)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
		BleFunc_SendResultToRx(reply_cmd, 0x01);
		return;
	}
	BleFunc_PrintTime6(&payload[0]);
	co_printf("    add_control=0x%02X\r\n", payload[6]);
	BleFunc_DumpPayload(payload, len);
	
	/* 下发到 MCU，由 MCU 负责 NFC 交互逻辑（Reply 0x00/0x01/0x02/0x03/0x04 + UUID + Key） */
	#if (!ENABLE_NFC_ADD_SIMULATION)
	{
		uint8_t conidx = Protocol_Get_Rx_Conidx();
		BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, (uint16_t)cmd, payload, (uint16_t)len);
	}
	#else
	/* [DEBUG-ONLY] 先保留模拟成功逻辑，同时镜像下发 MCU 便于联调对照 */
	BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
	co_printf("[DEBUG_SIM] Add NFC Success (0x00)\r\n");
	BleFunc_SendResultToRx(reply_cmd, 0x00);
	#endif
}

/**
 * @brief 5.12 手机删实体 NFC 卡（APP -> 设备）
 * @details
 * - Request Cmd: 0x0CFE
 * - Payload:
 *   - Time(6) + delete_control(1) + uuid(4) + key(6)   (delete_control=1)
 *   - Time(6) + delete_control(1)                     (delete_control=2/3)
 * - Response Cmd: 0x0C01
 * - Response Payload: ResultCode(1)
 */
void BleFunc_FE_DeleteNfc(uint16_t cmd, const uint8_t* payload, uint8_t len)
{
	co_printf("  -> Delete NFC (0x%04X)\r\n", cmd);
	uint16_t reply_cmd = BleFunc_MakeReplyCmd_FE(cmd);
	if (!BleFunc_EnsureAuthed(reply_cmd)) return;

	/* 0x0CFE: Time(6) + delete_control(1) = 7 bytes (delete_control=2/3) */
	/* 0x0CFE: Time(6) + delete_control(1) + uuid(4) + key(6) = 17 bytes (delete_control=1) */
	if (payload == NULL || len < 7) {
		co_printf("    invalid payload len=%d (expect >= 7)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
		BleFunc_SendResultToRx(reply_cmd, 0x01);
		return;
	}

	uint8_t delete_control = payload[6];
	if (delete_control == 1 && len < 17) {
		co_printf("    invalid payload len=%d (expect >= 17 for delete_control=1)\r\n", (int)len);
		BleFunc_DumpPayload(payload, len);
		BleFunc_SendResultToRx(reply_cmd, 0x01);
		return;
	}

	BleFunc_PrintTime6(&payload[0]);
	co_printf("    delete_control=0x%02X\r\n", delete_control);
	BleFunc_DumpPayload(payload, len);

	/* 下发到 MCU，由 MCU 负责 NFC 交互逻辑 */
	#if (!ENABLE_NFC_ADD_SIMULATION)
	{
		uint8_t conidx = Protocol_Get_Rx_Conidx();
		BleFunc_McuTxn_Start(conidx, reply_cmd, BLEFUNC_MCU_FEATURE, (uint16_t)cmd, payload, (uint16_t)len);
	}
	#else
	/* [DEBUG-ONLY] 模拟 MCU 回复成功（同时镜像下发 MCU 便于联调对照） */
	BleFunc_McuUart_SendOnly(BLEFUNC_MCU_FEATURE, cmd, payload, (uint16_t)len);
	co_printf("[DEBUG_SIM] Delete NFC Success (0x00)\r\n");
	BleFunc_SendResultToRx(reply_cmd, 0x00);
	#endif
}