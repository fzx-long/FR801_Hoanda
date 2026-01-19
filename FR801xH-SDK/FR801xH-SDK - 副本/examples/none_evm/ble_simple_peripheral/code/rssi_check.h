/**
 * @file rssi_check.h
 * @brief RSSI 信号强度滤波与判定模块头文件（结构体仿 class）
 */

#ifndef RSSI_CHECK_H
#define RSSI_CHECK_H

#include <stdint.h>
#include <stdbool.h>

#include "os_timer.h"

/* 最大连接数：如需调整，建议在编译选项或公共头文件中先定义 */
#ifndef RSSI_MAX_CONN
#define RSSI_MAX_CONN 3
#endif

/*
 * RSSI 打印开关：默认关闭，避免实时 RSSI 回调持续刷屏。
 * 如需打开，可在工程 C 预定义宏里设置 RSSI_PRINT_ENABLE=1
 */
#ifndef RSSI_PRINT_ENABLE
#define RSSI_PRINT_ENABLE 0
#endif

/**
 * @brief RSSI 获取回调函数类型（平台抽象接口）
 * @param conidx 连接索引
 * @return RSSI 值（负数，单位 dBm）
 */
typedef int8_t (*rssi_getter_t)(uint8_t conidx);

/**
 * @brief RSSI 距离状态变化回调
 *
 * @why
 * - RSSI 采样来自 GAP 的实时回调 `gap_rssi_ind()`，属于底层异步事件。
 * - 业务层（例如无感解锁）通常只关心“状态变化”（FAR/LOST -> NEAR），
 *   因此这里提供一个轻量的回调出口，避免业务代码侵入 RSSI 算法内部。
 *
 * @note
 * - 回调可能在 RAM 回调环境里触发，请保持回调实现足够轻量。
 */
typedef void (*rssi_distance_change_cb_t)(uint8_t conidx,
									 uint8_t new_distance,
									 int16_t filtered_rssi,
									 int8_t raw_rssi);

/**
 * @brief RSSI 距离状态变化回调
 *
 * @why
 * - RSSI 采样来自 GAP 的实时回调 `gap_rssi_ind()`，属于底层异步事件。
 * - 业务层（例如无感解锁）通常只关心“状态变化”（FAR/LOST -> NEAR），
 *   因此这里提供一个轻量的回调出口，避免业务代码侵入 RSSI 算法内部。
 *
 * @note
 * - 回调可能在 RAM 回调环境里触发，请保持回调实现足够轻量。
 */
typedef void (*rssi_distance_change_cb_t)(uint8_t conidx,
									 uint8_t new_distance,
									 int16_t filtered_rssi,
									 int8_t raw_rssi);

typedef enum {
	RSSI_DIST_LOST = 0,
	RSSI_DIST_FAR  = 1,
	RSSI_DIST_NEAR = 2,
} rssi_distance_t;

struct RSSI_Checker;

typedef struct {
	/* 反向指针：用于定时器回调里从 ctx 找回 self */
	struct RSSI_Checker *owner;

	bool            active;
	uint8_t         conidx;
	rssi_getter_t   getter;

	/* 3 点平均 */
	int8_t          s3_buf[3];
	uint8_t         s3_cnt;
	uint8_t         s3_idx;

	/* EMA（整数实现） */
	int16_t         ema;
	bool            ema_inited;

	rssi_distance_t distance;

	/* 对端 MAC（用于打印 RSSI+MAC） */
	uint8_t         peer_addr[6];
	bool            peer_addr_valid;

	/* os_timer_t 在本 SDK 中是结构体，不是指针：用标志位管理启动/停止 */
	bool            timer_inited;
	bool            timer_started;
	os_timer_t      timer;
} RSSI_ConnCtx;

typedef struct {
	/* 阈值（单位 dBm，负数） */
	int8_t  near_threshold;
	int8_t  far_threshold;
	int8_t  lost_threshold;
	uint8_t hysteresis;
	uint16_t period_ms;
} RSSI_Config;

typedef struct {
	void (*Init)(struct RSSI_Checker *self);
	void (*Enable)(struct RSSI_Checker *self, uint8_t conidx, rssi_getter_t getter);
	void (*Disable)(struct RSSI_Checker *self, uint8_t conidx);
	void (*Update)(struct RSSI_Checker *self, uint8_t conidx, int8_t rssi);
	int16_t (*GetFiltered)(struct RSSI_Checker *self, uint8_t conidx);
	uint8_t (*GetDistance)(struct RSSI_Checker *self, uint8_t conidx);
	bool (*IsNear)(struct RSSI_Checker *self, uint8_t conidx);
	bool (*IsLost)(struct RSSI_Checker *self, uint8_t conidx);
	void (*Reset)(struct RSSI_Checker *self, uint8_t conidx);
} RSSI_Methods;

typedef struct RSSI_Checker {
	RSSI_Methods m;
	RSSI_Config  cfg;
	RSSI_ConnCtx conn[RSSI_MAX_CONN];
} RSSI_Checker;

/**
 * @brief 构造一个 RSSI_Checker 对象（绑定方法表 + 默认配置）
 */
void RSSI_Checker_Construct(RSSI_Checker *self);

/**
 * @brief 获取默认全局对象（方便兼容旧调用方式）
 */
RSSI_Checker *RSSI_Get_Default(void);

/**
 * @brief 初始化 RSSI 检测模块
 */
void RSSI_Check_Init(void);

/**
 * @brief 启用指定连接的 RSSI 跟踪
 * @param conidx 连接索引
 * @param getter RSSI 获取回调函数（可为 NULL：使用 gap_rssi_ind/GAP 事件喂入 RSSI）
 */
void RSSI_Check_Enable(uint8_t conidx, rssi_getter_t getter);

/**
 * @brief 禁用指定连接的 RSSI 跟踪
 * @param conidx 连接索引
 */
void RSSI_Check_Disable(uint8_t conidx);

/**
 * @brief 更新指定连接的 RSSI 值（喂入新采样）
 * @param conidx 连接索引
 * @param rssi   原始 RSSI 值（负数，单位 dBm）
 */
void RSSI_Check_Update(uint8_t conidx, int8_t rssi);

/**
 * @brief 获取指定连接的滤波后 RSSI
 * @param conidx 连接索引
 * @return 滤波后 RSSI（dBm）
 */
int16_t RSSI_Check_Get_Filtered(uint8_t conidx);

/**
 * @brief 获取指定连接的距离状态
 * @param conidx 连接索引
 * @return 0=失联，1=远距离，2=近距离
 */
uint8_t RSSI_Check_Get_Distance(uint8_t conidx);

/**
 * @brief 判断是否近距离
 * @param conidx 连接索引
 * @return true=近距离，false=其他
 */
bool RSSI_Check_Is_Near(uint8_t conidx);

/**
 * @brief 判断是否失联
 * @param conidx 连接索引
 * @return true=失联，false=其他
 */
bool RSSI_Check_Is_Lost(uint8_t conidx);

/**
 * @brief 重置指定连接的滤波器
 * @param conidx 连接索引
 */
void RSSI_Check_Reset(uint8_t conidx);

/**
 * @brief 设置指定连接的对端 MAC 地址（连接建立时调用）
 * @param conidx 连接索引
 * @param addr6  6 字节 MAC（指向数组的指针）
 */
/**
 * @brief 获取指定连接的对端 MAC 地址
 * @param conidx     连接索引
 * @param out_addr6  输出 6 字节 MAC 缓冲区
 * @return true=有有效 MAC；false=无效/未保存
 */
bool RSSI_Check_Get_Peer_Addr(uint8_t conidx, uint8_t *out_addr6);

void RSSI_Check_Set_Peer_Addr(uint8_t conidx, const uint8_t *addr6);

/**
 * @brief 清除指定连接的对端 MAC 地址（断开连接时调用）
 * @param conidx 连接索引
 */
void RSSI_Check_Clear_Peer_Addr(uint8_t conidx);

/**
 * @brief 设置“距离状态变化”回调
 * @param cb 回调函数；传 NULL 表示关闭回调
 */
void RSSI_Check_Set_DistanceChangeCb(rssi_distance_change_cb_t cb);

/**
 * @brief 设置“距离状态变化”回调
 * @param cb 回调函数；传 NULL 表示关闭回调
 */
void RSSI_Check_Set_DistanceChangeCb(rssi_distance_change_cb_t cb);

#endif // RSSI_CHECK_H
