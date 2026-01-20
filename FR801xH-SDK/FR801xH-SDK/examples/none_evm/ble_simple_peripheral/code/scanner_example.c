/**
 * @file scanner_example.c
 * @brief BLE Scanner 使用示例（集成到 proj_main.c 的参考代码）
 * @version 0.1
 * @date 2025-12-31
 * 
 * @note 这不是完整的源文件，只是代码片段示例！
 */

/* ============ 第 1 步：在 proj_main.c 头部添加 ============ */
#include "scanner.h"

/* ============ 第 2 步：定义全局扫描器实例 ============ */
static BLE_Central_Base_t g_ble_scanner;

/* ============ 第 3 步：在 app_gap_evt_cb() 开头添加 ============ */
void app_gap_evt_cb(gap_event_t* p_event) {
    // 添加这一行，让 Scanner 处理 GAP 事件
    BLE_Scanner_Event_Handler(p_event);

    // ... 原有的 switch (p_event->type) ...
}

/* ============ 第 4 步：在 user_entry_after_ble_init() 中初始化 ============ */
void user_entry_after_ble_init(void) {
    // ... 原有的代码 ...

    // 创建扫描器实例
    BLE_Scanner_Create(&g_ble_scanner);

    // 开始扫描
    g_ble_scanner.ops.scan_start();

    // ... 原有的代码 ...
}

/* ============ 第 5 步：使用示例（在需要的地方调用）============ */

void example_usage(void) {
    // 开始扫描
    g_ble_scanner.ops.scan_start();

    // 停止扫描
    g_ble_scanner.ops.scan_stop();

    // 连接指定 MAC 地址的设备（小端序）
    uint8_t target_mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    g_ble_scanner.ops.connect(target_mac);

    // 断开连接
    g_ble_scanner.ops.disconnect();

    // 获取 RSSI（打印到串口）
    g_ble_scanner.ops.get_rssi();

    // 获取 MAC（打印到串口）
    g_ble_scanner.ops.get_mac();

    // 获取广播数据
    uint8_t  adv_data[31];
    uint8_t* p_adv = g_ble_scanner.ops.get_adv_data(adv_data);
    // 现在 adv_data 中有广播数据，p_adv 指向内部缓存

    // 直接访问状态变量
    if (g_ble_scanner.scaned) {
        co_printf("Scanned MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                  g_ble_scanner.scan_mac[5],
                  g_ble_scanner.scan_mac[4],
                  g_ble_scanner.scan_mac[3],
                  g_ble_scanner.scan_mac[2],
                  g_ble_scanner.scan_mac[1],
                  g_ble_scanner.scan_mac[0]);
        co_printf("RSSI: %d dBm\r\n", (int8_t)g_ble_scanner.rssi);
    }
}

/* ============ 注意事项 ============ */

/*
 * 1. 确保在 Keil 项目中添加 scanner.c 到编译列表
 * 
 * 2. 扫描和广播不能同时进行（硬件限制），如果你的设备当前在广播（peripheral 角色），
 *    开始扫描前需要先停止广播：gap_stop_advertising()
 * 
 * 3. MAC 地址是小端序（BLE 标准），例如实际地址 AA:BB:CC:DD:EE:FF 在数组中是：
 *    {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA}
 * 
 * 4. RSSI 是有符号整数（int8_t），通常范围 -100 ~ -30 dBm
 * 
 * 5. 连接操作是异步的，需要等待 GAP_EVT_MASTER_CONNECT 事件确认连接成功
 * 
 * 6. 如果需要在扫描时过滤特定设备，可以在 scanner_gap_event_handler() 的
 *    GAP_EVT_ADV_REPORT 分支中添加 MAC/广播数据过滤逻辑
 */
