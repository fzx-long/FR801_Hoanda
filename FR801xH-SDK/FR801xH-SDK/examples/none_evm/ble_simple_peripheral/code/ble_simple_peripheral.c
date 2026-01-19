/**
 * Copyright (c) 2019, Freqchip
 * 
 * All rights reserved.
 * 
 * 
 */
 
 /*
 * INCLUDES (����ͷ�ļ�)
 */
#include <stdbool.h>
#include <string.h>
#include "os_timer.h"
#include "gap_api.h"
#include "gatt_api.h"
#include "driver_gpio.h"
#include "simple_gatt_service.h"
#include "ble_simple_peripheral.h"
#include "protocol.h"
#include "scanner.h"
#include "TPMS.h"
#include "rssi_check.h"
#include "ble_function.h"

#include "sys_utils.h"
#include "flash_usage_config.h"

/* 允许同时连接的手机数量（中心设备数量） */
#define SP_MAX_CONN_NUM 3

/* 1: 使用固定名称 Wi-Box_79B736 (测试用); 0: 使用动态名称 Moto_MAC */
#define USE_TEST_FIXED_NAME 1

/* 记录每条链路是否已完成加密（Bond 后重连通常会自动加密） */
static uint8_t g_link_encrypted[SP_MAX_CONN_NUM] = {0};
/*
 * MACROS (�궨��)
 */

/*
 * CONSTANTS (��������)
 */

// GAP - Advertisement data (max size = 31 bytes, though this is
// best kept short to conserve power while advertisting)
// GAP-�㲥��������,�31���ֽ�.��һ������ݿ��Խ�ʡ�㲥ʱ��ϵͳ����.
static uint8_t adv_data[] =
{
  // service UUID, to notify central devices what services are included
  // in this peripheral. ����central������ʲô����, ��������ֻ��һ����Ҫ��.
    0x05,   // length of this data
    GAP_ADVTYPE_16BIT_MORE,      // some of the UUID's, but not all
    0xF0,
    0xFF,
    0x12,
    0x18,

    // Appearance: Keyboard (0x03C1)
    0x03,
    0x19,
    0xC1,
    0x03,
};

// GAP - Scan response data (max size = 31 bytes, though this is
// best kept short to conserve power while advertisting)
// GAP-Scan response����,�31���ֽ�.��һ������ݿ��Խ�ʡ�㲥ʱ��ϵͳ����.
static uint8_t scan_rsp_data[31] = {0};
static uint8_t scan_rsp_data_len = 0;
static uint8_t g_local_name[16] = {0}; /* "Moto_" + 6 hex + '\0' */

static uint8_t sp_hex_digit(uint8_t v)
{
    return (uint8_t)((v < 10) ? ('0' + v) : ('A' + (v - 10)));
}

static void sp_print_local_identity(const char *tag)
{
    mac_addr_t addr;
    gap_address_get(&addr);

    co_printf("[%s] Local BDADDR: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
              tag,
              addr.addr[5], addr.addr[4], addr.addr[3], addr.addr[2], addr.addr[1], addr.addr[0]);
    co_printf("[%s] Local Name: %s\r\n", tag, (char *)g_local_name);
}

static void sp_build_local_name_and_scan_rsp(void)
{
    mac_addr_t addr;
    gap_address_get(&addr);

#if USE_TEST_FIXED_NAME
    /* 命名规则：Wi-Box_46CC06 (Hardcoded as requested) */
    memcpy(g_local_name, "Wi-Box_46CC06", 13);
    g_local_name[13] = '\0';
#else
    /* 命名规则：Moto_MAC（MAC 为本机 BDADDR 显示串的后 6 位）
     * 这里按工程打印顺序 addr[0]..addr[5]，取后 3 字节 addr[3..5]。
     */
    g_local_name[0] = 'M';
    g_local_name[1] = 'o';
    g_local_name[2] = 't';
    g_local_name[3] = 'o';
    g_local_name[4] = '_';

    uint8_t b0 = addr.addr[3];
    uint8_t b1 = addr.addr[4];
    uint8_t b2 = addr.addr[5];

    g_local_name[5]  = sp_hex_digit((uint8_t)(b0 >> 4));
    g_local_name[6]  = sp_hex_digit((uint8_t)(b0 & 0x0F));
    g_local_name[7]  = sp_hex_digit((uint8_t)(b1 >> 4));
    g_local_name[8]  = sp_hex_digit((uint8_t)(b1 & 0x0F));
    g_local_name[9]  = sp_hex_digit((uint8_t)(b2 >> 4));
    g_local_name[10] = sp_hex_digit((uint8_t)(b2 & 0x0F));
    g_local_name[11] = '\0';
#endif

    uint8_t idx = 0;
    uint8_t name_len = (uint8_t)strlen((char *)g_local_name);
    if (name_len > 29) {
        name_len = 29;
    }
    scan_rsp_data[idx++] = (uint8_t)(name_len + 1);
    scan_rsp_data[idx++] = GAP_ADVTYPE_LOCAL_NAME_COMPLETE;
    memcpy(&scan_rsp_data[idx], g_local_name, name_len);
    idx = (uint8_t)(idx + name_len);

    scan_rsp_data[idx++] = 0x02;
    scan_rsp_data[idx++] = GAP_ADVTYPE_POWER_LEVEL;
    scan_rsp_data[idx++] = 0;

    scan_rsp_data_len = idx;
}

/*
 * TYPEDEFS (���Ͷ���)
 */

/*
 * GLOBAL VARIABLES (ȫ�ֱ���)
 */

/*
 * LOCAL VARIABLES (���ر���)
 */
os_timer_t update_param_timer;

/* BLE Scanner 实例（仅被动扫描） */
static BLE_Central_Base_t g_ble_scanner;

/*
 * @why
 * - 你反馈“MCU 一条 RSSI 都没收到”，根因很可能是：RSSI 实际上报走 GAP_EVT_LINK_RSSI，
 *   但应用层此前仅打印，没有把 RSSI 喂给 RSSI_Check_Update()，导致 NEAR 判定与下发逻辑不触发。
 * - 同时为了满足“RSSI 一直发送”，这里用定时器周期性调用 gap_get_link_rssi() 触发事件上报。
 */
#define RSSI_REQ_PERIOD_MS 500
static os_timer_t g_rssi_req_timer;
static uint8_t g_link_connected[SP_MAX_CONN_NUM] = {0};

static void sp_rssi_req_timer_func(void *arg)
{
    (void)arg;
    for (uint8_t i = 0; i < SP_MAX_CONN_NUM; i++) {
        if (g_link_connected[i]) {
            gap_get_link_rssi(i);
        }
    }
}

static void sp_rssi_req_timer_update(void)
{
    uint8_t any = 0;
    for (uint8_t i = 0; i < SP_MAX_CONN_NUM; i++) {
        if (g_link_connected[i]) {
            any = 1;
            break;
        }
    }

    if (any) {
        os_timer_start(&g_rssi_req_timer, RSSI_REQ_PERIOD_MS, 1);
    } else {
        os_timer_stop(&g_rssi_req_timer);
    }
}

/* 当前是否还有空位继续广播，允许多个中心设备接入 */
static void sp_try_continue_adv(void)
{
    if (gap_get_connect_num() < SP_MAX_CONN_NUM) {
        gap_start_advertising(0);
    }
}


 
/*
 * LOCAL FUNCTIONS (���غ���)
 */
static void sp_start_adv(void);
/*
 * EXTERN FUNCTIONS (�ⲿ����)
 */

/*
 * PUBLIC FUNCTIONS (ȫ�ֺ���)
 */

/** @function group ble peripheral device APIs (ble������ص�API)
 * @{
 */

void param_timer_func(void *arg)
{
    co_printf("param_timer_func\r\n");
    gap_conn_param_update(0, 12, 12, 55, 600);
}
/*********************************************************************
 * @fn      app_gap_evt_cb
 *
 * @brief   Application layer GAP event callback function. Handles GAP evnets.
 *
 * @param   p_event - GAP events from BLE stack.
 *       
 *
 * @return  None.
 */
void app_gap_evt_cb(gap_event_t *p_event)
{
    // 先交给 Scanner 处理（双角色：扫描事件）
    BLE_Scanner_Event_Handler(p_event);
    
    // 再处理 Peripheral 自己的事件
    switch(p_event->type)
    {
        case GAP_EVT_ADV_END:
        {
            co_printf("adv_end,status:0x%02x\r\n",p_event->param.adv_end.status);
            sp_try_continue_adv();
            
        }
        break;
        
        case GAP_EVT_ALL_SVC_ADDED:
        {
            co_printf("All service added\r\n");
            sp_start_adv();
        }
        break;

        case GAP_EVT_SLAVE_CONNECT:
        {
            co_printf("slave[%d],connect. link_num:%d\r\n",p_event->param.slave_connect.conidx,gap_get_connect_num());
            os_timer_start(&update_param_timer,4000,0);

            co_printf("peer[%d] addr: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                      p_event->param.slave_connect.conidx,
                      p_event->param.slave_connect.peer_addr.addr[5],
                      p_event->param.slave_connect.peer_addr.addr[4],
                      p_event->param.slave_connect.peer_addr.addr[3],
                      p_event->param.slave_connect.peer_addr.addr[2],
                      p_event->param.slave_connect.peer_addr.addr[1],
                      p_event->param.slave_connect.peer_addr.addr[0]);

            /* 新连接先标记为未加密，然后主动触发安全请求。
             * 这样：
             * - 首次连接会触发手机弹出配对（Bond）
             * - 已配对设备重连会更快进入加密态（无感解锁的前提）
             */
            if (p_event->param.slave_connect.conidx < SP_MAX_CONN_NUM)
            {
                g_link_encrypted[p_event->param.slave_connect.conidx] = 0;
            }
            co_printf("sec_req[%d] -> gap_security_req()\r\n", p_event->param.slave_connect.conidx);
            gap_security_req(p_event->param.slave_connect.conidx);

            /* 新连接先清理鉴权状态，等待 APP 发送 Token */
            Protocol_Auth_Clear(p_event->param.slave_connect.conidx);

            /* 启用 RSSI 滤波（事件驱动：RSSI 由 gap_rssi_ind 喂入） */
            RSSI_Check_Enable(p_event->param.slave_connect.conidx, NULL);

            /* 缓存对端 MAC，便于 RSSI 回调里打印 RSSI+MAC */
            RSSI_Check_Set_Peer_Addr(p_event->param.slave_connect.conidx,
                                     p_event->param.slave_connect.peer_addr.addr);
            gap_get_link_rssi(p_event->param.slave_connect.conidx);

            /* 标记连接有效，并开启 RSSI 轮询请求（保证持续产生 GAP_EVT_LINK_RSSI） */
            if (p_event->param.slave_connect.conidx < SP_MAX_CONN_NUM) {
                g_link_connected[p_event->param.slave_connect.conidx] = 1;
                sp_rssi_req_timer_update();
            }

            /* 还未达上限则继续广播，允许其它手机继续连接 */
            sp_try_continue_adv();
        }
        break;

        case GAP_EVT_DISCONNECT:
        {
            co_printf("Link[%d] disconnect,reason:0x%02X\r\n",p_event->param.disconnect.conidx
                      ,p_event->param.disconnect.reason);
					  os_timer_stop(&update_param_timer);

            if (p_event->param.disconnect.conidx < SP_MAX_CONN_NUM)
            {
                g_link_encrypted[p_event->param.disconnect.conidx] = 0;
            }

            /* 断链清理鉴权状态 */
            Protocol_Auth_Clear(p_event->param.disconnect.conidx);

            /* 断开后关闭该连接的 RSSI 跟踪 */
            RSSI_Check_Disable(p_event->param.disconnect.conidx);
            RSSI_Check_Clear_Peer_Addr(p_event->param.disconnect.conidx);

            /* 清理连接标记，并在无连接时停止 RSSI 轮询请求 */
            if (p_event->param.disconnect.conidx < SP_MAX_CONN_NUM) {
                g_link_connected[p_event->param.disconnect.conidx] = 0;
                sp_rssi_req_timer_update();
            }

            sp_try_continue_adv();
        }
        break;

        case GAP_EVT_LINK_PARAM_REJECT:
            co_printf("Link[%d]param reject,status:0x%02x\r\n"
                      ,p_event->param.link_reject.conidx,p_event->param.link_reject.status);
            break;

        case GAP_EVT_LINK_PARAM_UPDATE:
            co_printf("Link[%d]param update,interval:%d,latency:%d,timeout:%d\r\n",p_event->param.link_update.conidx
                      ,p_event->param.link_update.con_interval,p_event->param.link_update.con_latency,p_event->param.link_update.sup_to);
            break;

        case GAP_EVT_PEER_FEATURE:
            co_printf("peer[%d] feats ind\r\n",p_event->param.peer_feature.conidx);
            //show_reg((uint8_t *)&(p_event->param.peer_feature.features),8,1);
            break;

        case GAP_EVT_MTU:
            co_printf("mtu update,conidx=%d,mtu=%d\r\n"
                      ,p_event->param.mtu.conidx,p_event->param.mtu.value);
            break;
        
        case GAP_EVT_LINK_RSSI:
            /*
             * @why
             * - gap_get_link_rssi() 的回包是 GAP_EVT_LINK_RSSI（SDK 注释明确）。
             * - 这里必须喂给 RSSI_Check_Update()，否则 NEAR 判定与 MCU 下发不会触发。
             */
            RSSI_Check_Update(p_event->conidx, p_event->param.link_rssi);

            /* 你要求：采集到 RSSI 就发 MCU（不做阈值处理） */
            BleFunc_RSSI_RawInd(p_event->conidx, p_event->param.link_rssi);
            break;
                
        case GAP_SEC_EVT_SLAVE_ENCRYPT:
            co_printf("slave[%d]_encrypted\r\n",p_event->param.slave_encrypt_conidx);
						os_timer_start(&update_param_timer,4000,0);

                        if (p_event->param.slave_encrypt_conidx < SP_MAX_CONN_NUM)
                        {
                                g_link_encrypted[p_event->param.slave_encrypt_conidx] = 1;
                        }

                        co_printf("enc_state[%d]=1\r\n", p_event->param.slave_encrypt_conidx);
				
            break;

        default:
            break;
    }
}

/*********************************************************************
 * @fn      sp_start_adv
 *
 * @brief   Set advertising data & scan response & advertising parameters and start advertising
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
static void sp_start_adv(void)
{
    sp_print_local_identity("ADV");

    // Set advertising parameters
    gap_adv_param_t adv_param;
    adv_param.adv_mode = GAP_ADV_MODE_UNDIRECT;
    adv_param.adv_addr_type = GAP_ADDR_TYPE_PUBLIC;
    adv_param.adv_chnl_map = GAP_ADV_CHAN_ALL;
    adv_param.adv_filt_policy = GAP_ADV_ALLOW_SCAN_ANY_CON_ANY;
    adv_param.adv_intv_min = 600;
    adv_param.adv_intv_max = 600;
        
    gap_set_advertising_param(&adv_param);
    
    // Set advertising data & scan response data
	gap_set_advertising_data(adv_data, sizeof(adv_data));
    gap_set_advertising_rsp_data(scan_rsp_data, scan_rsp_data_len);
    // Start advertising
	co_printf("Start advertising...\r\n");
	gap_start_advertising(0);
}


/*********************************************************************
 * @fn      simple_peripheral_init
 *
 * @brief   Initialize simple peripheral profile, BLE related parameters.
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
void simple_peripheral_init(void)
{
    /* 生成 Moto_MAC 设备名，并同步到 Device Name + Scan Response */
    sp_build_local_name_and_scan_rsp();
    gap_set_dev_name(g_local_name, (uint8_t)(strlen((char *)g_local_name) + 1));

    sp_print_local_identity("BOOT");

    os_timer_init( &update_param_timer,param_timer_func,NULL);

    /* RSSI 轮询请求定时器：用于周期性触发 gap_get_link_rssi() */
    os_timer_init(&g_rssi_req_timer, sp_rssi_req_timer_func, NULL);
    memset(g_link_connected, 0, sizeof(g_link_connected));

#if 0		//security encryption
    gap_security_param_t param =
    {
        .mitm = true,		
        .ble_secure_conn = true,		//not enable security encryption
        .io_cap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT,		//ble device has input ability, will input pin code. 
        .pair_init_mode = GAP_PAIRING_MODE_WAIT_FOR_REQ,	//need bond
        .bond_auth = true,	//need bond auth
    };
#endif 
#if 0
    gap_security_param_t param =
    {
        .mitm = true,		// use PIN code during bonding
        .ble_secure_conn = false,		//not enable security encryption
        .io_cap = GAP_IO_CAP_KEYBOARD_ONLY,		//ble device has input ability, will input pin code. 
        .pair_init_mode = GAP_PAIRING_MODE_WAIT_FOR_REQ,	//need bond
        .bond_auth = true,	//need bond auth
    };
#endif
#if 0
    gap_security_param_t param =
    {
        .mitm = true,		// use PIN code during bonding
        .ble_secure_conn = false,		//not enable security encryption
        .io_cap = GAP_IO_CAP_DISPLAY_ONLY,	//ble device has output ability, will show pin code. 
        .pair_init_mode = GAP_PAIRING_MODE_WAIT_FOR_REQ, //need bond
        .bond_auth = true,	//need bond auth
        .password = 123456,	//set PIN code, it is a dec num between 100000 ~ 999999
    };
#endif
#if 1
    gap_security_param_t param =
    {
        .mitm = false,	// dont use PIN code during bonding
        .ble_secure_conn = false,	//not enable security encryption
        .io_cap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT, //ble device has neither output nor input ability, 
        .pair_init_mode = GAP_PAIRING_MODE_WAIT_FOR_REQ,		//need bond
        .bond_auth = true,	//need bond auth
        .password = 0,
    };
#endif
		// Initialize security related settings.
    gap_security_param_init(&param);
    
    gap_set_cb_func(app_gap_evt_cb);

        /* RSSI 过滤模块初始化 + 使能底层实时 RSSI 上报 */
        RSSI_Check_Init();
        gap_set_link_rssi_report(true);
        /*
         * @why
         * - 你要求“采集到 RSSI 就发 MCU”，因此不再使用距离状态(NEAR/FAR/LOST)门禁逻辑。
         * - GAP_EVT_LINK_RSSI 里已经每次都调用 BleFunc_RSSI_RawInd() 下发 MCU。
         */
        RSSI_Check_Set_DistanceChangeCb(NULL);
    
    /* 初始化 TPMS：仅保留绑定加载/学习与 Flash 存储 */
    TPMS_Init();

    /* 初始化 BLE Scanner （双角色：扫描功能）*/
    BLE_Scanner_Create(&g_ble_scanner);
    co_printf("[Peripheral] Scanner initialized, starting scan...\r\n");
    g_ble_scanner.ops.scan_start();  // 启动扫描

		//enable bond manage module, which will record bond key and peer service info into flash. 
		//and read these info from flash when func: "gap_bond_manager_init" executes.
    gap_bond_manager_init(BLE_BONDING_INFO_SAVE_ADDR, BLE_REMOTE_SERVICE_SAVE_ADDR, 8, true);
    //gap_bond_manager_delete_all();

    /* 再打印一次：确认 bond manager init 后地址仍稳定 */
    sp_print_local_identity("BOND");
    
    // Adding services to database
    sp_gatt_add_service();  
    
    // Initialize Protocol
    Protocol_Init();
}


