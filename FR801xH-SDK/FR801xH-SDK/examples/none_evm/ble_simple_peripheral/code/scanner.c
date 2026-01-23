/**
 * @file scanner.c
 * @brief BLE 中心设备扫描器实现（OOP 风格）
 * @version 0.1
 * @date 2025-12-31
 */
#include "scanner.h"
#include "gap_api.h"
#include "co_printf.h"
#include "TPMS.h" // TPMS 广播解析
#include <string.h>

static bool scanner_adv_name_is_tpms(const uint8_t* data, uint8_t len)
{
    if (data == NULL || len == 0u)
        return false;
    uint8_t idx = 0u;
    while (idx < len)
    {
        uint8_t l = data[idx];
        if (l == 0u)
            break;
        uint8_t end = (uint8_t)(idx + 1u + l);
        if (end > len)
        {
            break;
        }
        if (l < 1u)
        {
            break;
        }
        uint8_t type = data[idx + 1u];
        if (type == 0x09u || type == 0x08u)
        {
            uint8_t name_len = (uint8_t)(l - 1u);
            if (name_len == 5u && memcmp(&data[idx + 2u], "TPMSS", 5u) == 0)
            {
                return true;
            }
        }
        idx = end;
    }
    return false;
}

/* 日志开关：0=静音，1=打开 */
#ifndef SCAN_LOG_ENABLE
#define SCAN_LOG_ENABLE 0
#endif

#if SCAN_LOG_ENABLE
#define SCAN_LOG(...) co_printf(__VA_ARGS__)
#else
#define SCAN_LOG(...)                                                          \
    do                                                                         \
    {                                                                          \
    } while (0)
#endif

/* ============ 内部状态管理 ============ */
static BLE_Central_Base_t* g_scanner_instance = NULL; // 全局实例指针
static uint8_t             g_adv_data_cache[31];      // 广播数据缓存
static uint8_t             g_adv_data_len = 0;

/* ============ 私有函数声明 ============ */
static void scanner_gap_event_handler(gap_event_t* p_event);

/* ============ SDK 层函数实现 ============ */

/**
 * @brief 开始扫描
 * @note  使用保守参数以支持双角色（广播+扫描同时运行）
 */
static void scanner_start_scan(void)
{
    if (g_scanner_instance == NULL)
    {
        SCAN_LOG("[Scanner] Error: Instance not initialized.\r\n");
        return;
    }

    gap_scan_param_t scan_param;
    scan_param.scan_mode   = GAP_SCAN_MODE_GEN_DISC; // 通用发现模式
    scan_param.scan_intv   = 160; // 扫描间隔 160*0.625ms = 100ms
    scan_param.scan_window = 32;  // 扫描窗口 32*0.625ms = 20ms
    scan_param.duration    = 0;   // 0 = 持续扫描（直到调用 gap_stop_scan）

    gap_start_scan(&scan_param);
    SCAN_LOG("[Scanner] Scan started (continuous).\r\n");
}

/**
 * @brief 停止扫描
 */
static void scanner_stop_scan(void)
{
    gap_stop_scan();
    SCAN_LOG("[Scanner] Scan stopped.\r\n");
}

/**
 * @brief 获取当前连接的 RSSI
 */
static void scanner_get_rssi(void)
{
    if (g_scanner_instance == NULL)
    {
        SCAN_LOG("[Scanner] Error: Instance not initialized.\r\n");
        return;
    }

    SCAN_LOG("[Scanner] Last RSSI: %d dBm\r\n",
             (int8_t)g_scanner_instance->rssi);
}

/**
 * @brief 获取当前扫描到的 MAC 地址
 */
static void scanner_get_mac(void)
{
    if (g_scanner_instance == NULL)
    {
        SCAN_LOG("[Scanner] Error: Instance not initialized.\r\n");
        return;
    }

    uint8_t* mac = g_scanner_instance->scan_mac;
    SCAN_LOG("[Scanner] Last scanned MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
             mac[5],
             mac[4],
             mac[3],
             mac[2],
             mac[1],
             mac[0]);
}

/**
 * @brief 获取广播数据
 * @param data 输出缓冲区指针（由调用者分配，至少 31 字节）
 * @return 返回广播数据的指针（指向内部缓存）
 */
static uint8_t* scanner_get_adv_data(uint8_t* data)
{
    if (data == NULL)
    {
        SCAN_LOG("[Scanner] Error: Invalid buffer pointer.\r\n");
        return NULL;
    }

    memcpy(data, g_adv_data_cache, g_adv_data_len);
    SCAN_LOG("[Scanner] ADV data length: %d bytes\r\n", g_adv_data_len);

    return g_adv_data_cache;
}

/* ============ GAP 事件回调处理 ============ */

/**
 * @brief GAP 事件处理函数（需要在 app_gap_evt_cb 中调用）
 * @param p_event GAP 事件指针
 */
static void scanner_gap_event_handler(gap_event_t* p_event)
{
    if (g_scanner_instance == NULL || p_event == NULL)
    {
        return;
    }

    switch (p_event->type)
    {
    case GAP_EVT_ADV_REPORT: // 收到广播数据
    {
        gap_evt_adv_report_t* adv = p_event->param.adv_rpt;
        if (adv == NULL)
        {
            break;
        }

        // 更新状态
        g_scanner_instance->scaned = 1;
        memcpy(g_scanner_instance->scan_mac, adv->src_addr.addr.addr, 6);
        g_scanner_instance->rssi = adv->rssi;

        // 缓存广播/扫描回应数据
        if (adv->length <= 31)
        {
            memcpy(g_adv_data_cache, adv->data, adv->length);
            g_adv_data_len = adv->length;
        }
        else
        {
            g_adv_data_len = 0;
        }

        if (scanner_adv_name_is_tpms(g_adv_data_cache, g_adv_data_len))
        {
            SCAN_LOG("[TPMS_SCAN] name=TPMSS mac=%02X:%02X:%02X:%02X:%02X:%02X "
                     "rssi=%d len=%u\r\n",
                     adv->src_addr.addr.addr[5],
                     adv->src_addr.addr.addr[4],
                     adv->src_addr.addr.addr[3],
                     adv->src_addr.addr.addr[2],
                     adv->src_addr.addr.addr[1],
                     adv->src_addr.addr.addr[0],
                     (int)adv->rssi,
                     (unsigned)adv->length);
        }

        // 将广播数据交给 TPMS 模块解析（不打印 MAC/RSSI）
        TPMS_Feed_Adv(adv->src_addr.addr.addr,
                      (int8_t)adv->rssi,
                      g_adv_data_cache,
                      g_adv_data_len);
        break;
    }

    case GAP_EVT_SCAN_END: // 扫描结束（异常或外部 stop）
    {
        SCAN_LOG("[Scanner] Scan ended. Restarting...\r\n");
        g_scanner_instance->scaned = 0;
        // 保护性重启，确保持续扫描
        scanner_start_scan();
        break;
    }

    default: break;
    }
}

/* ============ 构造/析构函数 ============ */

/**
 * @brief 创建并初始化 BLE Central 实例
 * @param device 设备结构体指针（由调用者分配内存）
 */
void BLE_Scanner_Create(BLE_Central_Base_t* device)
{
    if (device == NULL)
    {
        co_printf("[Scanner] Error: Device pointer is NULL.\r\n");
        return;
    }

    // 初始化状态
    device->scaned = 0;
    memset(device->scan_mac, 0, 6);
    device->rssi = 0;

    // 绑定操作函数
    device->ops.scan_start   = scanner_start_scan;
    device->ops.scan_stop    = scanner_stop_scan;
    device->ops.get_rssi     = scanner_get_rssi;
    device->ops.get_mac      = scanner_get_mac;
    device->ops.get_adv_data = scanner_get_adv_data;

    // 设置全局实例
    g_scanner_instance = device;

    co_printf("[Scanner] Instance created successfully.\r\n");
}

/**
 * @brief 注册到 GAP 事件回调（需要在 app_gap_evt_cb 中调用）
 * @param p_event GAP 事件指针
 */
void BLE_Scanner_Event_Handler(gap_event_t* p_event)
{
    scanner_gap_event_handler(p_event);
}
