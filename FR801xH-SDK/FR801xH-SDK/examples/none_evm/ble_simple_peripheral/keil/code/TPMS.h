/**
 * @file TPMS.h
 * @brief TPMS 广播解析接口
 *
 * @details
 * 本模块负责：
 * - 接收扫描器上报的 BLE ADV/SCAN_RSP 原始数据
 * - 从 AD Type 0xFF (Manufacturer Specific) 中解析 TPMS 传感器自定义数据段
 * - 缓存每个传感器的最近一次广播及解析后的结构化字段，便于上层读取/打印
 */
#ifndef TPMS_H
#define TPMS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 支持的 TPMS 传感器数量（当前示例为 2 个轮位/传感器）
 */
#define TPMS_SENSOR_MAX 2

/**
 * @brief TPMS 电池状态（来自 status 高 4bit）
 */
typedef enum {
	TPMS_BATT_STATUS_INVALID = 0,
	TPMS_BATT_STATUS_NORMAL,
	TPMS_BATT_STATUS_LOW,
} TPMS_BattStatus;

/**
 * @brief TPMS 模式（来自 status 低 4bit）
 */
typedef enum {
	TPMS_MODE_INVALID = 0,
	TPMS_MODE_PARK,
	TPMS_MODE_DRIVING_ALARM,
	TPMS_MODE_DRIVING,
	TPMS_MODE_PARK_ALARM,
} TPMS_Mode;

/**
 * @brief 解析后的 TPMS 广播数据（对应 AD Type 0xFF 的自定义数据段）
 * @note
 * - pressure_kpa_x100：单位 0.01kPa（文档精度 3.14kPa -> raw * 314）
 * - battery_v_x100：单位 0.01V（文档 v=raw*0.01 + 1.22 -> raw + 122）
 */
typedef struct {
	bool valid;
	uint8_t header;              /**< 固定 0x4D */
	uint8_t status_raw;          /**< 原始 status */
	TPMS_BattStatus batt_status; /**< 电池状态 */
	TPMS_Mode mode;              /**< 模式 */
	uint32_t rolling_cnt;        /**< 滚动计数（每包+1） */
	uint32_t sensor_id;          /**< 传感器 ID */
	uint16_t pressure_kpa_x100;  /**< 压力 */
	int8_t temperature_c;        /**< 温度（摄氏度） */
	uint16_t battery_v_x100;     /**< 电池电压 */
	uint8_t bt_addr[6];          /**< 广播里携带的蓝牙地址字段（文档 Byte26~31） */
} TPMS_Frame;

/**
 * @brief 传感器缓存槽（最近一次广播与解析结果）
 */
typedef struct {
	uint8_t mac[6];        /**< 传感器 MAC（小端存放，与扫描器上报一致） */
	int8_t  rssi;          /**< 最近一次广播 RSSI */
	uint8_t data[31];      /**< 最近一次广播原始数据（ADV/SCAN_RSP payload） */
	uint8_t data_len;      /**< data 的有效长度 */
	TPMS_Frame frame;      /**< 解析后的结构化字段 */
	bool    valid;         /**< 该槽位是否已被学习/填充 */
} TPMS_Sensor;

typedef struct tpms_ops_t {
	void (*set_target)(uint8_t wheel_idx, const uint8_t mac_le[6]);
	void (*feed_adv)(const uint8_t mac_le[6], int8_t rssi, const uint8_t *data, uint8_t len);
	void (*debug_print)(void);
} tpms_ops_t;

typedef struct {
	tpms_ops_t ops;
	TPMS_Sensor sensor[TPMS_SENSOR_MAX];
	uint8_t target_mac[TPMS_SENSOR_MAX][6];
} TPMS_Device;

/**
 * @brief 初始化 TPMS 模块
 */
void TPMS_Init(void);

/**
 * @brief 设置指定轮位/槽位绑定的目标传感器 MAC
 * @param wheel_idx 轮位索引（0 ~ TPMS_SENSOR_MAX-1）
 * @param mac_le 目标 MAC（小端，6 字节；与扫描器上报一致）
 */
void TPMS_Set_Target(uint8_t wheel_idx, const uint8_t mac_le[6]);

/**
 * @brief 喂入一帧扫描到的广播数据
 * @param mac_le 广播源 MAC（小端，6 字节）
 * @param rssi RSSI
 * @param data 广播数据（0~31 字节）
 * @param len data 长度
 */
void TPMS_Feed_Adv(const uint8_t mac_le[6], int8_t rssi, const uint8_t *data, uint8_t len);

/**
 * @brief 获取指定轮位的传感器数据
 * @param wheel_idx 轮位索引（0 ~ TPMS_SENSOR_MAX-1）
 * @param out_frame 输出结构体指针
 * @return true=获取成功（该轮位已绑定且有有效数据），false=失败
 */
bool TPMS_Get_Sensor(uint8_t wheel_idx, TPMS_Frame *out_frame);

/**
 * @brief 打印 TPMS 模块当前缓存/解析状态（用于联调）
 */
void TPMS_Debug_Print(void);

/**
 * @brief 进入“更换/学习”模式：在指定时间窗口内自动选择候选并绑定到轮位
 *
 * @details
 * - 调用后会清空该轮位原绑定（避免旧 sensor_id 继续生效）
 * - 在学习窗口内统计收到的 TPMS 传感器候选（按包数/最大 RSSI 选最优）
 * - 窗口结束后写入 flash 并生效
 *
 * @param wheel_idx 轮位索引（0 ~ TPMS_SENSOR_MAX-1）
 * @param window_ms 学习窗口长度（ms），建议 10000~30000
 */
void TPMS_Replace_Start(uint8_t wheel_idx, uint32_t window_ms);

/**
 * @brief 清空指定轮位绑定（并写入 flash）
 * @param wheel_idx 轮位索引
 */
void TPMS_Clear_Binding(uint8_t wheel_idx);

/**
 * @brief 清空所有轮位绑定（并写入 flash）
 */
void TPMS_Clear_All_Bindings(void);

/**
 * @brief 打印当前绑定表（使用 co_printf 输出，便于串口联调）
 */
void TPMS_Binding_Print(void);

/**
 * @brief 解析 TPMS 广播（BLE ADV/SCAN_RSP 数据段）
 * @param adv 广播数据（0~31 字节）
 * @param adv_len 长度
 * @param out 输出结构体
 * @return true=解析成功（找到并解析到 0xFF 段）；false=未命中/长度不合法
 */
bool TPMS_Parse_Adv(const uint8_t *adv, uint8_t adv_len, TPMS_Frame *out);

#endif // TPMS_H

