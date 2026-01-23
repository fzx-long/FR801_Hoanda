/**
 * @file TPMS.c
 * @brief TPMS 广播接收/缓存/解析实现
 *
 * @details
 * 数据来源：扫描器上报的 BLE ADV/SCAN_RSP payload（0~31 字节）。
 * 解析方法：遍历 BLE Advertising Data（AD）结构，定位 AD Type 0xFF
 * (Manufacturer Specific)，并按协议文档解析出压力/温度/电压等字段。
 *
 * 说明：本模块默认支持 2 个传感器槽位（见 TPMS_SENSOR_MAX），并提供“学习”逻辑：
 * - 若未显式绑定目标 MAC，则遇到新 MAC 会自动占用空槽位
 * - 若已绑定目标 MAC，则优先匹配绑定的 MAC
 */

#include "TPMS.h"
#include "flash_usage_config.h"
#include "co_printf.h"
#include "driver_flash.h"
#include "os_timer.h"
#include <string.h>
#include "usart_cmd.h"
#include "usart_device.h"

/**
 * @name BLE AD 类型定义
 * @{ 
 */
#ifndef BLE_AD_TYPE_COMPLETE_LOCAL_NAME
#define BLE_AD_TYPE_COMPLETE_LOCAL_NAME 0x09
#endif

#ifndef BLE_AD_TYPE_MANUFACTURER_SPECIFIC
#define BLE_AD_TYPE_MANUFACTURER_SPECIFIC 0xFF
#endif
/** @ */

/**
 * @brief 文档定义：TPMS 自定义 Manufacturer 数据段固定头
 */
#ifndef TPMS_PAYLOAD_HEADER
#define TPMS_PAYLOAD_HEADER 0x4D
#endif

/**
 * @brief 日志开关：0=静音，1=打开
 * @note 建议联调阶段打开，量产阶段关闭以节省串口带宽与 CPU
 */
#ifndef TPMS_LOG_ENABLE
#define TPMS_LOG_ENABLE 0
#endif

#if TPMS_LOG_ENABLE
#define TPMS_LOG(...) co_printf(__VA_ARGS__)
#else
#define TPMS_LOG(...) do {} while (0)
#endif

static bool mac_equal(const uint8_t a[6], const uint8_t b[6])
{
	return memcmp(a, b, 6) == 0;
}

static bool mac_all_ff(const uint8_t mac[6])
{
	static const uint8_t ff[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	return memcmp(mac, ff, 6) == 0;
}

static bool mac_all_00(const uint8_t mac[6])
{
	static const uint8_t z[6] = {0};
	return memcmp(mac, z, 6) == 0;
}

static void copy_mac(uint8_t dst[6], const uint8_t src[6])
{
	memcpy(dst, src, 6);
}


static void tpms_dump_hex(const uint8_t *p, uint8_t len)
{
    if (p == NULL) {
        TPMS_LOG("[TPMS] ADV: null\r\n");
        return;
    }
    TPMS_LOG("[TPMS] ADV len=%u: ", (unsigned)len);
    for (uint8_t i = 0; i < len; i++) {
        TPMS_LOG("%02X ", p[i]);
    }
    TPMS_LOG("\r\n");
}

static bool tpms_adv_name_is_tpmss(const uint8_t *adv, uint8_t adv_len)
{
    if (adv == NULL || adv_len == 0u) return false;
    uint8_t idx = 0u;
    while (idx < adv_len) {
        uint8_t l = adv[idx];
        if (l == 0u) break;
        uint8_t end = (uint8_t)(idx + 1u + l);
        if (end > adv_len || l < 1u) {
            break;
        }
        uint8_t type = adv[idx + 1u];
        if (type == 0x09u || type == 0x08u) {
            uint8_t name_len = (uint8_t)(l - 1u);
            if (name_len == 5u && memcmp(&adv[idx + 2u], "TPMSS", 5u) == 0) {
                return true;
            }
        }
        idx = end;
    }
    return false;
}

static TPMS_Device g_tpms;

typedef struct {
	bool valid;
	uint32_t sensor_id;
	uint8_t mac_le[6];
} tpms_binding_t;

static tpms_binding_t g_tpms_binding[TPMS_SENSOR_MAX];

/*********************************************************************
 * @brief CRC16/CCITT-FALSE (poly=0x1021, init=0xFFFF)，用于校验绑定存储块。
 */
static uint16_t tpms_crc16_ccitt_false(const uint8_t *data, uint32_t len)
{
	uint16_t crc = 0xFFFF;
	for (uint32_t i = 0; i < len; i++) {
		crc ^= (uint16_t)data[i] << 8;
		for (uint8_t b = 0; b < 8; b++) {
			crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
		}
	}
	return crc;
}

/*********************************************************************
 * @brief Flash 存储体：单扇区保存绑定表（含头/版本/长度/CRC16）。
 * @note  写入需整扇区擦除；读取需校验 magic/version/size 与 CRC16。
 */
typedef struct {
	uint32_t magic;   /* 'TPMS' */
	uint16_t version; /* 1 */
	uint16_t size;
	struct {
		uint8_t valid;
		uint8_t rsv0[3];
		uint32_t sensor_id;
		uint8_t mac_le[6];
		uint8_t rsv1[2];
	} entry[TPMS_SENSOR_MAX];
	uint16_t crc16;
	uint16_t rsv2;
} tpms_bind_store_t;

#ifndef TPMS_BIND_STORE_MAGIC
#define TPMS_BIND_STORE_MAGIC 0x534D5054u /* 'TPMS' little-end */
#endif

static void tpms_binding_set(uint8_t wheel_idx, uint32_t sensor_id, const uint8_t mac_le[6])
{
	if (wheel_idx >= TPMS_SENSOR_MAX || mac_le == NULL) return;
	g_tpms_binding[wheel_idx].valid = true;
	g_tpms_binding[wheel_idx].sensor_id = sensor_id;
	memcpy(g_tpms_binding[wheel_idx].mac_le, mac_le, 6);
	copy_mac(g_tpms.target_mac[wheel_idx], mac_le);
}

static void tpms_binding_clear(uint8_t wheel_idx)
{
	if (wheel_idx >= TPMS_SENSOR_MAX) return;
	memset(&g_tpms_binding[wheel_idx], 0, sizeof(g_tpms_binding[wheel_idx]));
	for (int j = 0; j < 6; j++) {
		g_tpms.target_mac[wheel_idx][j] = 0xFF;
	}
}

static bool tpms_binding_any_valid(void)
{
	for (int i = 0; i < TPMS_SENSOR_MAX; i++) {
		if (g_tpms_binding[i].valid) return true;
	}
	return false;
}

/*
 * Flash 存储说明：
 * - 存储结构 tpms_bind_store_t 带 magic/version/size 和 CRC16，避免脏数据误读
 * - 读取：直接 flash_read 到结构体，校验 magic/version/size 和 CRC16 后才生效
 * - 写入：先填充结构体并计算 CRC16 -> 如开启 FLASH_PROTECT 则先 disable -> flash_erase 扇区 -> flash_write 全结构
 */
static void tpms_bind_store_load(void)
{
	tpms_bind_store_t st;
	memset(&st, 0, sizeof(st));
	flash_read(TPMS_BINDING_INFO_SAVE_ADDR, sizeof(st), (uint8_t *)&st);

	/* 基本头校验，避免误把未初始化/损坏的数据当成有效绑定 */
	if (st.magic != TPMS_BIND_STORE_MAGIC || st.version != 1u || st.size != (uint16_t)sizeof(st)) {
		return;
	}
	uint16_t calc = tpms_crc16_ccitt_false((const uint8_t *)&st, (uint32_t)(sizeof(st) - sizeof(st.crc16) - sizeof(st.rsv2)));
	if (calc != st.crc16) {
		return;
	}

	/* 校验通过后恢复各轮位绑定 */
	for (int i = 0; i < TPMS_SENSOR_MAX; i++) {
		if (st.entry[i].valid) {
			tpms_binding_set((uint8_t)i, st.entry[i].sensor_id, st.entry[i].mac_le);
		}
	}
}

static void tpms_bind_store_save(void)
{
	tpms_bind_store_t st;
	memset(&st, 0, sizeof(st));
	st.magic = TPMS_BIND_STORE_MAGIC;
	st.version = 1u;
	st.size = (uint16_t)sizeof(st);
	for (int i = 0; i < TPMS_SENSOR_MAX; i++) {
		st.entry[i].valid = g_tpms_binding[i].valid ? 1u : 0u;
		st.entry[i].sensor_id = g_tpms_binding[i].sensor_id;
		memcpy(st.entry[i].mac_le, g_tpms_binding[i].mac_le, 6);
	}
	st.crc16 = tpms_crc16_ccitt_false((const uint8_t *)&st, (uint32_t)(sizeof(st) - sizeof(st.crc16) - sizeof(st.rsv2)));

	/* 擦除-写入流程：必须先擦除 4KB 扇区，再整体写入结构体 */
#ifdef FLASH_PROTECT
	flash_protect_disable(1);
#endif
	flash_erase(TPMS_BINDING_INFO_SAVE_ADDR, 0x1000);
	flash_write(TPMS_BINDING_INFO_SAVE_ADDR, sizeof(st), (uint8_t *)&st);
#ifdef FLASH_PROTECT
	flash_protect_enable(1);
#endif
}

/* Replace/Learn 状态机：使用定时器窗口选中候选 */
#ifndef TPMS_LEARN_CAND_MAX
#define TPMS_LEARN_CAND_MAX 8
#endif

typedef struct {
	bool active;
	uint8_t wheel_idx;
	uint32_t packet_cnt;
	int8_t max_rssi;
	uint32_t sensor_id;
	uint8_t mac_le[6];
} tpms_learn_cand_t;

static struct {
	bool active;
	uint8_t wheel_idx;
	os_timer_t timer;
	tpms_learn_cand_t cand[TPMS_LEARN_CAND_MAX];
} g_tpms_learn;

static void tpms_learn_reset_candidates(void)
{
	memset(g_tpms_learn.cand, 0, sizeof(g_tpms_learn.cand));
	for (int i = 0; i < TPMS_LEARN_CAND_MAX; i++) {
		g_tpms_learn.cand[i].max_rssi = -127;
	}
}

static tpms_learn_cand_t *tpms_learn_find_or_alloc(uint32_t sensor_id, const uint8_t mac_le[6])
{
	for (int i = 0; i < TPMS_LEARN_CAND_MAX; i++) {
		if (g_tpms_learn.cand[i].active && g_tpms_learn.cand[i].sensor_id == sensor_id) {
			return &g_tpms_learn.cand[i];
		}
	}
	for (int i = 0; i < TPMS_LEARN_CAND_MAX; i++) {
		if (!g_tpms_learn.cand[i].active) {
			g_tpms_learn.cand[i].active = true;
			g_tpms_learn.cand[i].wheel_idx = g_tpms_learn.wheel_idx;
			g_tpms_learn.cand[i].sensor_id = sensor_id;
			g_tpms_learn.cand[i].packet_cnt = 0;
			g_tpms_learn.cand[i].max_rssi = -127;
			memcpy(g_tpms_learn.cand[i].mac_le, mac_le, 6);
			return &g_tpms_learn.cand[i];
		}
	}

	/* 列表满：替换最弱候选（packet_cnt 最小，其次 max_rssi 最小） */
	int worst = 0;
	for (int i = 1; i < TPMS_LEARN_CAND_MAX; i++) {
		if (g_tpms_learn.cand[i].packet_cnt < g_tpms_learn.cand[worst].packet_cnt) {
			worst = i;
		} else if (g_tpms_learn.cand[i].packet_cnt == g_tpms_learn.cand[worst].packet_cnt
				   && g_tpms_learn.cand[i].max_rssi < g_tpms_learn.cand[worst].max_rssi) {
			worst = i;
		}
	}
	g_tpms_learn.cand[worst].active = true;
	g_tpms_learn.cand[worst].wheel_idx = g_tpms_learn.wheel_idx;
	g_tpms_learn.cand[worst].sensor_id = sensor_id;
	g_tpms_learn.cand[worst].packet_cnt = 0;
	g_tpms_learn.cand[worst].max_rssi = -127;
	memcpy(g_tpms_learn.cand[worst].mac_le, mac_le, 6);
	return &g_tpms_learn.cand[worst];
}

static void tpms_learn_timer_cb(void *arg)
{
	(void)arg;
	if (!g_tpms_learn.active) return;

	/* 选择最优候选：包数最大，其次最大 RSSI */
	int best = -1;
	for (int i = 0; i < TPMS_LEARN_CAND_MAX; i++) {
		if (!g_tpms_learn.cand[i].active) continue;
		if (best < 0) {
			best = i;
			continue;
		}
		if (g_tpms_learn.cand[i].packet_cnt > g_tpms_learn.cand[best].packet_cnt) {
			best = i;
		} else if (g_tpms_learn.cand[i].packet_cnt == g_tpms_learn.cand[best].packet_cnt
				   && g_tpms_learn.cand[i].max_rssi > g_tpms_learn.cand[best].max_rssi) {
			best = i;
		}
	}

	if (best >= 0) {
		uint8_t w = g_tpms_learn.wheel_idx;
		tpms_binding_set(w, g_tpms_learn.cand[best].sensor_id, g_tpms_learn.cand[best].mac_le);
		tpms_bind_store_save();
		co_printf("[TPMS] LEARN DONE wheel=%u id=0x%08X mac=%02X:%02X:%02X:%02X:%02X:%02X cnt=%u rssi=%d\r\n",
				  (unsigned)w,
				  (unsigned)g_tpms_binding[w].sensor_id,
				  g_tpms_binding[w].mac_le[5], g_tpms_binding[w].mac_le[4], g_tpms_binding[w].mac_le[3],
				  g_tpms_binding[w].mac_le[2], g_tpms_binding[w].mac_le[1], g_tpms_binding[w].mac_le[0],
				  (unsigned)g_tpms_learn.cand[best].packet_cnt,
				  (int)g_tpms_learn.cand[best].max_rssi);
	} else {
		co_printf("[TPMS] LEARN FAIL: no candidate\r\n");
	}

	g_tpms_learn.active = false;
}

/**
 * @brief 从目标列表/学习槽位中选择一个槽位
 * @param mac_le 广播源 MAC（小端）
 * @return 槽位索引（0~TPMS_SENSOR_MAX-1），失败返回 -1
 */
static int pick_slot_by_frame(const TPMS_Frame *f, const uint8_t mac_le[6])
{
	if (f == NULL || mac_le == NULL || !f->valid) return -1;

	/* 1) 已绑定：优先按 sensor_id 匹配，其次按 MAC 匹配（兼容旧绑定） */
	for (int i = 0; i < TPMS_SENSOR_MAX; i++) {
		if (g_tpms_binding[i].valid && g_tpms_binding[i].sensor_id == f->sensor_id) {
			return i;
		}
	}
	for (int i = 0; i < TPMS_SENSOR_MAX; i++) {
		if (g_tpms_binding[i].valid && mac_equal(g_tpms_binding[i].mac_le, mac_le)) {
			return i;
		}
	}

	/* 2) 未绑定且不在学习：保留原“自动学习”策略（仅当完全无绑定时启用） */
	if (!g_tpms_learn.active && !tpms_binding_any_valid()) {
		for (int i = 0; i < TPMS_SENSOR_MAX; i++) {
			if (mac_all_ff(g_tpms.target_mac[i]) || mac_all_00(g_tpms.target_mac[i])) {
				copy_mac(g_tpms.target_mac[i], mac_le);
				TPMS_LOG("[TPMS] learn slot%d MAC=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
						 i, mac_le[5], mac_le[4], mac_le[3], mac_le[2], mac_le[1], mac_le[0]);
				return i;
			}
		}
	}

	return -1;
}

/**
 * @brief 设置指定槽位绑定的目标 MAC
 */
static void tpms_set_target_mac(uint8_t wheel_idx, const uint8_t mac_le[6])
{
	if (wheel_idx >= TPMS_SENSOR_MAX || mac_le == NULL) return;
	copy_mac(g_tpms.target_mac[wheel_idx], mac_le);
}

/**
 * @brief 接收一帧广播并更新对应槽位缓存
 *
 * @details
 * - 会缓存原始数据、RSSI、MAC
 * - 会尝试调用 TPMS_Parse_Adv() 解析 Manufacturer 段，解析失败时 frame.valid=false
 */
static void tpms_feed_adv(const uint8_t mac_le[6], int8_t rssi, const uint8_t *data, uint8_t len)
{
    TPMS_LOG("[TPMS_FEED] mac=%02X:%02X:%02X:%02X:%02X:%02X rssi=%d len=%u\r\n",
             mac_le ? mac_le[5] : 0, mac_le ? mac_le[4] : 0, mac_le ? mac_le[3] : 0,
             mac_le ? mac_le[2] : 0, mac_le ? mac_le[1] : 0, mac_le ? mac_le[0] : 0,
             (int)rssi, (unsigned)len);
    tpms_dump_hex(data, len);

	if (mac_le == NULL || data == NULL || len == 0 || len > 31) return;

	TPMS_Frame frame;
	if (!TPMS_Parse_Adv(data, len, &frame)) {

		return; /* 非 TPMS 广播直接忽略，避免误学习 */
	}

#if 1 /* Pass-through Mode: Directly Send to UART */
    /* Resp payload: Wheel(1)+ID(4)+Press(2)+Temp(1)+Volt(2)+Status(1)+MAC(6) = 17 Bytes */
    uint8_t report_buf[18];
    
    report_buf[0] = 0xFF; // Wheel Index Unknown (No binding check)
    
    /* ID (4B Big Endian) */
    report_buf[1] = (uint8_t)((frame.sensor_id >> 24) & 0xFF);
    report_buf[2] = (uint8_t)((frame.sensor_id >> 16) & 0xFF);
    report_buf[3] = (uint8_t)((frame.sensor_id >> 8) & 0xFF);
    report_buf[4] = (uint8_t)(frame.sensor_id & 0xFF);
    
    /* Pressure (2B Big Endian) */
    report_buf[5] = (uint8_t)((frame.pressure_kpa_x100 >> 8) & 0xFF);
    report_buf[6] = (uint8_t)(frame.pressure_kpa_x100 & 0xFF);
    
    /* Temp (1B) */
    report_buf[7] = (uint8_t)frame.temperature_c;
    
    /* Voltage (2B Big Endian) */
    report_buf[8] = (uint8_t)((frame.battery_v_x100 >> 8) & 0xFF);
    report_buf[9] = (uint8_t)(frame.battery_v_x100 & 0xFF);
    
    /* Status (1B) */
    report_buf[10] = frame.status_raw;
    
    /* MAC (6B) */
    memcpy(&report_buf[11], frame.bt_addr, 6);

	/*RSSI(1B)*/
    report_buf[17] = (uint8_t)rssi;

    /* Send via UART as CMD (SOC -> MCU) */
    SocMcu_Frame_Send(SOC_MCU_SYNC_SOC_TO_MCU, 
                      SOC_MCU_FEATURE_FF01, 
                      CMD_Tire_pressure_monitoring_get, 
                      report_buf, 
                      sizeof(report_buf));
                      
    /* [DEBUG] 日志打印 */
    TPMS_LOG("[TPMS RAW UART] ID=0x%08X P=%d.%02dkPa\r\n",
             (unsigned)frame.sensor_id,
             (int)(frame.pressure_kpa_x100 / 100), (int)(frame.pressure_kpa_x100 % 100));

    return; /* Skip original logic */
#endif
}

/**
 * @brief 打印当前所有槽位状态（仅在 TPMS_LOG_ENABLE=1 时输出）
 */
static void tpms_print_status(void)
{
	for (int i = 0; i < TPMS_SENSOR_MAX; i++) {
		TPMS_Sensor *s = &g_tpms.sensor[i];
		if (!s->valid) {
			TPMS_LOG("[TPMS] slot%d: empty\r\n", i);
			continue;
		}
		TPMS_LOG("[TPMS] slot%d MAC=%02X:%02X:%02X:%02X:%02X:%02X RSSI=%d len=%d\r\n",
				 i, s->mac[5], s->mac[4], s->mac[3], s->mac[2], s->mac[1], s->mac[0],
				 s->rssi, s->data_len);
		if (s->frame.valid) {
			TPMS_LOG("[TPMS]   hdr=0x%02X status=0x%02X roll=%u id=0x%08X\r\n",
					 (unsigned)s->frame.header,
					 (unsigned)s->frame.status_raw,
					 (unsigned)s->frame.rolling_cnt,
					 (unsigned)s->frame.sensor_id);
			TPMS_LOG("[TPMS]   P=%d.%02dkPa T=%dC V=%d.%02dV bt=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
					 (int)(s->frame.pressure_kpa_x100 / 100),
					 (int)(s->frame.pressure_kpa_x100 % 100),
					 (int)s->frame.temperature_c,
					 (int)(s->frame.battery_v_x100 / 100),
					 (int)(s->frame.battery_v_x100 % 100),
					 s->frame.bt_addr[5], s->frame.bt_addr[4], s->frame.bt_addr[3],
					 s->frame.bt_addr[2], s->frame.bt_addr[1], s->frame.bt_addr[0]);
		}
	}
}

/**
 * @brief 4 字节小端转 uint32
 */
static uint32_t u32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint16_t u16_be(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t u32_le(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief status 高 4bit 解码为电池状态
 * @param status_high_nibble status[7:4]
 */
static TPMS_BattStatus tpms_decode_batt(uint8_t status_high_nibble)
{
	/* 文档：Bit7~Bit4：0xB=低电，0x9=正常，其它无效 */
	if (status_high_nibble == 0x9) return TPMS_BATT_STATUS_NORMAL;
	if (status_high_nibble == 0xB) return TPMS_BATT_STATUS_LOW;
	return TPMS_BATT_STATUS_INVALID;
}

/**
 * @brief status 低 4bit 解码为模式
 * @param mode_low_nibble status[3:0]
 */
static TPMS_Mode tpms_decode_mode(uint8_t mode_low_nibble)
{
	/* 文档：Bit3~Bit0：0x1=驻车，0x2=运行漏气，0x3=运行，0x6=驻车漏气 */
	switch (mode_low_nibble) {
		case 0x1: return TPMS_MODE_PARK;
		case 0x2: return TPMS_MODE_DRIVING_ALARM;
		case 0x3: return TPMS_MODE_DRIVING;
		case 0x6: return TPMS_MODE_PARK_ALARM;
		default:  return TPMS_MODE_INVALID;
	}
}

/**
 * @brief 解析 TPMS 广播（BLE ADV/SCAN_RSP 数据段）
 *
 * @details
 * BLE AD 结构为：len(1) + type(1) + payload(len-1)
 * - 本函数遍历所有 AD 结构，寻找 type=0xFF
 * - 文档约束：AD len=0x14（含 type），因此 payload_len=0x13，并且 payload[0]=0x4D
 * - 命中后解析字段并做单位换算
 *
 * @param adv 广播数据（0~31 字节）
 * @param adv_len 广播数据长度
 * @param out 输出结构体（函数会先清零）
 * @return true=解析成功（命中 0xFF 段并解析完成）；false=未命中/长度异常
 */
bool TPMS_Parse_Adv(const uint8_t *adv, uint8_t adv_len, TPMS_Frame *out)
{
	bool name_tpmss = tpms_adv_name_is_tpmss(adv, adv_len);

	if (out) {
		memset(out, 0, sizeof(*out));
	}
	if (adv == NULL || adv_len == 0 || adv_len > 31 || out == NULL) {
		return false;
	}

	/* BLE AD 格式：len(1) + type(1) + data(len-1)；重复直到结束 */
	uint8_t idx = 0;
	while (idx < adv_len) {
		uint8_t l = adv[idx];
		if (l == 0) {
			break;
		}
		uint8_t end = (uint8_t)(idx + 1 + l);
		if (end > adv_len) {
			/* 半包/异常长度：直接失败 */
			return false;
		}
		if (l < 1) {
			return false;
		}
		uint8_t type = adv[idx + 1];
		const uint8_t *payload = &adv[idx + 2];
		uint8_t payload_len = (uint8_t)(l - 1);

		if (type == BLE_AD_TYPE_MANUFACTURER_SPECIFIC) {
			/* 文档：AD Len=0x14 => payload_len=0x13=19，且第一个字节固定 0x4D */
			const uint8_t *p = payload;
			if (payload_len == 0x15 && payload[2] == TPMS_PAYLOAD_HEADER) {
				/* 兼容 Manufacturer Company ID(2B) */
				p = &payload[2];
			}
			if (payload_len == 0x13 && payload[0] == 0x93 && payload[1] == 0x4D) {
				const uint8_t *q = &payload[2];
				out->header = 0u;
				out->status_raw = 0u;
				out->batt_status = TPMS_BATT_STATUS_INVALID;
				out->mode = TPMS_MODE_INVALID;
				out->rolling_cnt = u32_le(&q[0]);
				out->sensor_id = u32_le(&q[4]);
				out->pressure_kpa_x100 = (uint16_t)((uint16_t)q[8] * 314u);
				out->temperature_c = (int8_t)q[9];
				out->battery_v_x100 = (uint16_t)((uint16_t)q[10] + 122u);
				memcpy(out->bt_addr, &q[11], 6);
				out->valid = true;
				TPMS_LOG("[TPMS_PARSE] ok id=0x%08X P=%u T=%d V=%u status=0x%02X mac=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
				         (unsigned)out->sensor_id, (unsigned)out->pressure_kpa_x100, (int)out->temperature_c,
				         (unsigned)out->battery_v_x100, (unsigned)out->status_raw,
				         out->bt_addr[5], out->bt_addr[4], out->bt_addr[3], out->bt_addr[2], out->bt_addr[1], out->bt_addr[0]);
				return true;
			}

			if (payload_len == 0x13 && payload[0] == 0x93 && payload[1] == 0x4D) {
				const uint8_t *q = &payload[2];
				out->header = 0u;
				out->status_raw = 0u;
				out->batt_status = TPMS_BATT_STATUS_INVALID;
				out->mode = TPMS_MODE_INVALID;
				out->rolling_cnt = u32_le(&q[0]);
				out->sensor_id = u32_le(&q[4]);
				out->pressure_kpa_x100 = (uint16_t)((uint16_t)q[8] * 314u);
				out->temperature_c = (int8_t)q[9];
				out->battery_v_x100 = (uint16_t)((uint16_t)q[10] + 122u);
				memcpy(out->bt_addr, &q[11], 6);
				out->valid = true;
				TPMS_LOG("[TPMS_PARSE] ok id=0x%08X P=%u T=%d V=%u status=0x%02X mac=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
				         (unsigned)out->sensor_id, (unsigned)out->pressure_kpa_x100, (int)out->temperature_c,
				         (unsigned)out->battery_v_x100, (unsigned)out->status_raw,
				         out->bt_addr[5], out->bt_addr[4], out->bt_addr[3], out->bt_addr[2], out->bt_addr[1], out->bt_addr[0]);
				return true;
			}

			if (name_tpmss && payload_len >= 17u && payload[0] == 0xFFu) {
				out->header = payload[0];
				out->status_raw = payload[10];
				out->batt_status = tpms_decode_batt((uint8_t)((out->status_raw >> 4) & 0x0F));
				out->mode = tpms_decode_mode((uint8_t)(out->status_raw & 0x0F));
				out->rolling_cnt = 0u;
				out->sensor_id = u32_be(&payload[1]);
				out->pressure_kpa_x100 = u16_be(&payload[5]);
				out->temperature_c = (int8_t)payload[7];
				out->battery_v_x100 = u16_be(&payload[8]);
				memcpy(out->bt_addr, &payload[11], 6);
				out->valid = true;
				TPMS_LOG("[TPMS_PARSE] ok id=0x%08X P=%u T=%d V=%u status=0x%02X mac=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
				         (unsigned)out->sensor_id, (unsigned)out->pressure_kpa_x100, (int)out->temperature_c,
				         (unsigned)out->battery_v_x100, (unsigned)out->status_raw,
				         out->bt_addr[5], out->bt_addr[4], out->bt_addr[3], out->bt_addr[2], out->bt_addr[1], out->bt_addr[0]);
				return true;
			}

			if (payload_len == 0x13 && p[0] == TPMS_PAYLOAD_HEADER) {
				out->header = p[0];
				out->status_raw = p[1];
				out->batt_status = tpms_decode_batt((uint8_t)((out->status_raw >> 4) & 0x0F));
				out->mode = tpms_decode_mode((uint8_t)(out->status_raw & 0x0F));
				out->rolling_cnt = u32_le(&p[2]);
				out->sensor_id = u32_le(&p[6]);
				/* 文档：压力 raw，精度 3.14kPa，偏移 0 => pressure_kpa_x100 = raw * 314 */
				out->pressure_kpa_x100 = (uint16_t)((uint16_t)p[10] * 314u);
				out->temperature_c = (int8_t)p[11];
				/* 文档：电池 raw，精度 0.01V，偏移 +1.22 => battery_v_x100 = raw + 122 */
				out->battery_v_x100 = (uint16_t)((uint16_t)p[12] + 122u);
				memcpy(out->bt_addr, &p[13], 6);
				out->valid = true;
				TPMS_LOG("[TPMS_PARSE] ok id=0x%08X P=%u T=%d V=%u status=0x%02X mac=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
				         (unsigned)out->sensor_id, (unsigned)out->pressure_kpa_x100, (int)out->temperature_c,
				         (unsigned)out->battery_v_x100, (unsigned)out->status_raw,
				         out->bt_addr[5], out->bt_addr[4], out->bt_addr[3], out->bt_addr[2], out->bt_addr[1], out->bt_addr[0]);
				return true;
			}
		}

		/* 其它 AD 类型先忽略（0x09 名称等），后续如需过滤可在这里加 */
		idx = end;
	}

	return false;
}

/**
 * @brief TPMS 模块初始化
 *
 * @details
 * - 清空全局状态
 * - 将 target_mac 填充为 0xFF，表示“未绑定目标 MAC，允许学习”
 */
void TPMS_Init(void)
{
	memset(&g_tpms, 0, sizeof(g_tpms));
	memset(&g_tpms_binding, 0, sizeof(g_tpms_binding));
	memset(&g_tpms_learn, 0, sizeof(g_tpms_learn));
	for (int i = 0; i < TPMS_SENSOR_MAX; i++) {
		for (int j = 0; j < 6; j++) {
			g_tpms.target_mac[i][j] = 0xFF;
		}
	}

	g_tpms.ops.set_target = tpms_set_target_mac;
	g_tpms.ops.feed_adv = tpms_feed_adv;
	g_tpms.ops.debug_print = tpms_print_status;

	os_timer_init(&g_tpms_learn.timer, tpms_learn_timer_cb, NULL);
	tpms_learn_reset_candidates();
	tpms_bind_store_load();

	TPMS_LOG("[TPMS] Init done. Waiting for ADV...\r\n");
}

void TPMS_Replace_Start(uint8_t wheel_idx, uint32_t window_ms)
{
	if (wheel_idx >= TPMS_SENSOR_MAX) {
		co_printf("[TPMS] REPLACE invalid wheel=%u\r\n", (unsigned)wheel_idx);
		return;
	}
	if (window_ms < 1000) window_ms = 20000;

	/* 清空旧绑定，进入学习窗口 */
	tpms_binding_clear(wheel_idx);
	tpms_bind_store_save();

	g_tpms_learn.active = true;
	g_tpms_learn.wheel_idx = wheel_idx;
	tpms_learn_reset_candidates();
	os_timer_start(&g_tpms_learn.timer, window_ms, false);

	co_printf("[TPMS] REPLACE START wheel=%u window=%ums\r\n", (unsigned)wheel_idx, (unsigned)window_ms);
}

void TPMS_Clear_Binding(uint8_t wheel_idx)
{
	if (wheel_idx >= TPMS_SENSOR_MAX) return;
	tpms_binding_clear(wheel_idx);
	tpms_bind_store_save();
	co_printf("[TPMS] CLEAR wheel=%u\r\n", (unsigned)wheel_idx);
}

void TPMS_Clear_All_Bindings(void)
{
	for (int i = 0; i < TPMS_SENSOR_MAX; i++) {
		tpms_binding_clear((uint8_t)i);
	}
	tpms_bind_store_save();
	co_printf("[TPMS] CLEAR ALL\r\n");
}

void TPMS_Binding_Print(void)
{
	co_printf("[TPMS] BINDINGS:\r\n");
	for (int i = 0; i < TPMS_SENSOR_MAX; i++) {
		if (g_tpms_binding[i].valid) {
			co_printf("  wheel=%d id=0x%08X mac=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
					  i,
					  (unsigned)g_tpms_binding[i].sensor_id,
					  g_tpms_binding[i].mac_le[5], g_tpms_binding[i].mac_le[4], g_tpms_binding[i].mac_le[3],
					  g_tpms_binding[i].mac_le[2], g_tpms_binding[i].mac_le[1], g_tpms_binding[i].mac_le[0]);
		} else {
			co_printf("  wheel=%d <empty>\r\n", i);
		}
	}
}

/**
 * @brief 设置目标轮胎传感器的 MAC 地址
 * 
 * @param wheel_idx 轮胎索引（0 到 TPMS_SENSOR_MAX-1）
 * @param mac_le 目标传感器的 MAC 地址（小端格式，6 字节）
 */
void TPMS_Set_Target(uint8_t wheel_idx, const uint8_t mac_le[6])
{
	if (g_tpms.ops.set_target) {
		g_tpms.ops.set_target(wheel_idx, mac_le);
	}
}
/**
 * @brief 喂入一帧扫描到的广播数据
 * @param mac_le 广播源 MAC（小端，6 字节）
 * @param rssi RSSI
 * @param data 广播数据（0~31 字节）
 * @param len data 长度
 */
void TPMS_Feed_Adv(const uint8_t mac_le[6], int8_t rssi, const uint8_t *data, uint8_t len)
{
	if (g_tpms.ops.feed_adv) {
		g_tpms.ops.feed_adv(mac_le, rssi, data, len);
	}
}

/**
 * @brief 打印 TPMS 模块的调试信息
 */
void TPMS_Debug_Print(void)
{
	if (g_tpms.ops.debug_print) {
		g_tpms.ops.debug_print();
	}
}

bool TPMS_Get_Sensor(uint8_t wheel_idx, TPMS_Frame *out_frame)
{
	if (wheel_idx >= TPMS_SENSOR_MAX || out_frame == NULL) return false;
	
	TPMS_Sensor *s = &g_tpms.sensor[wheel_idx];
	if (!s->valid || !s->frame.valid) TPMS_LOG("[TPMS_PARSE] fail\r\n");
		return false;

	*out_frame = s->frame;
	return true;
}
