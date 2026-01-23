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
#include <stdio.h>
#include <string.h>
#include "co_printf.h"
#include "gap_api.h"
#include "gatt_api.h"
#include "gatt_sig_uuid.h"

#include "simple_gatt_service.h"
#include "protocol.h"

/*
 * MACROS (�궨��)
 */

/*
 * CONSTANTS (��������)
 */

// Simple GATT Profile Service UUID: 0xFFF0 - But using 128-bit base
const uint8_t sp_svc_uuid[] = SP_SVC_UUID_128;

/******************************* Characteristic 1 defination *******************************/
// Characteristic 1 UUID: 0xFFF1
// Characteristic 1 data
#define SP_CHAR1_VALUE_LEN 20
uint8_t sp_char1_value[SP_CHAR1_VALUE_LEN] = {0};
// Characteristic 1 User Description
#define SP_CHAR1_DESC_LEN 17
const uint8_t sp_char1_desc[SP_CHAR1_DESC_LEN] = "Characteristic 1";

/******************************* Characteristic 2 defination *******************************/
// Characteristic 2 UUID: 0xFFF2
// Characteristic 2 data
#define SP_CHAR2_VALUE_LEN 20
uint8_t sp_char2_value[SP_CHAR2_VALUE_LEN] = {0};
// Characteristic 2 User Description
#define SP_CHAR2_DESC_LEN 17
const uint8_t sp_char2_desc[SP_CHAR2_DESC_LEN] = "Characteristic 2";

/******************************* Characteristic 3 defination *******************************/
// Characteristic 3 UUID: 0xFFF3
// Characteristic 3 data
#define SP_CHAR3_VALUE_LEN 30
uint8_t sp_char3_value[SP_CHAR3_VALUE_LEN] = {0};
// Characteristic 3 User Description
#define SP_CHAR3_DESC_LEN 17
const uint8_t sp_char3_desc[SP_CHAR3_DESC_LEN] = "Characteristic 3";

/******************************* Characteristic 4 defination *******************************/
/* 文档协议只需要：FFF6/FFF1/1004，移除多余特征值(FFF4/FFF5)。 */

/*
 * TYPEDEFS (���Ͷ���)
 */

/*
 * GLOBAL VARIABLES (ȫ�ֱ���)
 */
uint8_t sp_svc_id = 0;

/* HID over GATT (HOGP) service id */
static uint8_t hid_svc_id = 0;

/* 支持多连接：每个连接独立的通知使能状态 */
#define SP_MAX_CONN_NUM 3
static uint8_t ntf_char1_enable[SP_MAX_CONN_NUM] = {0};
static uint8_t ntf_char2_enable[SP_MAX_CONN_NUM] = {0};

/* HID input report notification enable per connection */
static uint8_t hid_in_ntf_enable[SP_MAX_CONN_NUM] = {0};

/* HID service (0x1812) */
static const uint8_t hid_svc_uuid[] = UUID16_ARR(0x1812);

/* HID characteristic values */
static uint8_t       hid_protocol_mode  = 0x01; /* 0=Boot, 1=Report */
static const uint8_t hid_information[4] = {
    0x11,
    0x01, /* bcdHID = 0x0111 */
    0x00, /* bCountryCode */
    0x03  /* flags: RemoteWake(1) | NormallyConnectable(1) */
};

/* Minimal keyboard report map (Report ID = 1) */
static const uint8_t hid_report_map[] = {
    0x05, 0x01, /* Usage Page (Generic Desktop) */
    0x09, 0x06, /* Usage (Keyboard) */
    0xA1, 0x01, /* Collection (Application) */
    0x85, 0x01, /*   Report ID (1) */
    0x05, 0x07, /*   Usage Page (Key Codes) */
    0x19, 0xE0, /*   Usage Minimum (224) */
    0x29, 0xE7, /*   Usage Maximum (231) */
    0x15, 0x00, /*   Logical Minimum (0) */
    0x25, 0x01, /*   Logical Maximum (1) */
    0x75, 0x01, /*   Report Size (1) */
    0x95, 0x08, /*   Report Count (8) */
    0x81, 0x02, /*   Input (Data, Variable, Absolute) ; Modifier byte */
    0x95, 0x01, /*   Report Count (1) */
    0x75, 0x08, /*   Report Size (8) */
    0x81, 0x01, /*   Input (Constant) ; Reserved byte */
    0x95, 0x05, /*   Report Count (5) */
    0x75, 0x01, /*   Report Size (1) */
    0x05, 0x08, /*   Usage Page (LEDs) */
    0x19, 0x01, /*   Usage Minimum (1) */
    0x29, 0x05, /*   Usage Maximum (5) */
    0x91, 0x02, /*   Output (Data, Variable, Absolute) ; LED report */
    0x95, 0x01, /*   Report Count (1) */
    0x75, 0x03, /*   Report Size (3) */
    0x91, 0x01, /*   Output (Constant) ; padding */
    0x95, 0x06, /*   Report Count (6) */
    0x75, 0x08, /*   Report Size (8) */
    0x15, 0x00, /*   Logical Minimum (0) */
    0x25, 0x65, /*   Logical Maximum (101) */
    0x05, 0x07, /*   Usage Page (Key Codes) */
    0x19, 0x00, /*   Usage Minimum (0) */
    0x29, 0x65, /*   Usage Maximum (101) */
    0x81, 0x00, /*   Input (Data, Array) ; Key arrays (6 bytes) */
    0xC0        /* End Collection */
};

/* Report values */
static uint8_t hid_input_report[9]  = {0}; /* [ID][mod][res][k1..k6] */
static uint8_t hid_output_report[2] = {0}; /* [ID][LEDs] */

/* Report Reference descriptors: [Report ID, Report Type] */
static const uint8_t hid_in_report_ref[2]  = {0x01, 0x01}; /* ID=1, Input */
static const uint8_t hid_out_report_ref[2] = {0x01, 0x02}; /* ID=1, Output */

/*
 * LOCAL VARIABLES (���ر���)
 */
static gatt_service_t simple_profile_svc;

/* HID service descriptor */
static gatt_service_t hid_profile_svc;

/* HID attribute index */
enum {
    HID_IDX_SERVICE,
    HID_IDX_HID_INFO_DECL,
    HID_IDX_HID_INFO_VAL,
    HID_IDX_REPORT_MAP_DECL,
    HID_IDX_REPORT_MAP_VAL,
    HID_IDX_PROTOCOL_MODE_DECL,
    HID_IDX_PROTOCOL_MODE_VAL,
    HID_IDX_CONTROL_POINT_DECL,
    HID_IDX_CONTROL_POINT_VAL,
    HID_IDX_REPORT_IN_DECL,
    HID_IDX_REPORT_IN_VAL,
    HID_IDX_REPORT_IN_CFG,
    HID_IDX_REPORT_IN_REF,
    HID_IDX_REPORT_OUT_DECL,
    HID_IDX_REPORT_OUT_VAL,
    HID_IDX_REPORT_OUT_REF,
    HID_IDX_NB,
};

static const gatt_attribute_t hid_profile_att_table[HID_IDX_NB] = {
    [HID_IDX_SERVICE] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_PRIMARY_SERVICE_UUID)},
            GATT_PROP_READ,
            UUID_SIZE_2,
            (uint8_t*)hid_svc_uuid,
        },

    [HID_IDX_HID_INFO_DECL] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)},
            GATT_PROP_READ,
            0,
            NULL,
        },
    [HID_IDX_HID_INFO_VAL] =
        {
            {UUID_SIZE_2, UUID16_ARR(0x2A4A)}, /* HID Information */
            GATT_PROP_READ,
            sizeof(hid_information),
            (uint8_t*)hid_information,
        },

    [HID_IDX_REPORT_MAP_DECL] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)},
            GATT_PROP_READ,
            0,
            NULL,
        },
    [HID_IDX_REPORT_MAP_VAL] =
        {
            {UUID_SIZE_2, UUID16_ARR(0x2A4B)}, /* Report Map */
            GATT_PROP_READ,
            sizeof(hid_report_map),
            (uint8_t*)hid_report_map,
        },

    [HID_IDX_PROTOCOL_MODE_DECL] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)},
            GATT_PROP_READ,
            0,
            NULL,
        },
    [HID_IDX_PROTOCOL_MODE_VAL] =
        {
            {UUID_SIZE_2, UUID16_ARR(0x2A4E)}, /* Protocol Mode */
            GATT_PROP_READ | GATT_PROP_WRITE,
            1,
            &hid_protocol_mode,
        },

    [HID_IDX_CONTROL_POINT_DECL] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)},
            GATT_PROP_READ,
            0,
            NULL,
        },
    [HID_IDX_CONTROL_POINT_VAL] =
        {
            {UUID_SIZE_2, UUID16_ARR(0x2A4C)}, /* HID Control Point */
            GATT_PROP_WRITE,
            1,
            NULL,
        },

    [HID_IDX_REPORT_IN_DECL] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)},
            GATT_PROP_READ,
            0,
            NULL,
        },
    [HID_IDX_REPORT_IN_VAL] =
        {
            {UUID_SIZE_2, UUID16_ARR(0x2A4D)}, /* Report (Input) */
            GATT_PROP_READ | GATT_PROP_NOTI,
            sizeof(hid_input_report),
            hid_input_report,
        },
    [HID_IDX_REPORT_IN_CFG] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CLIENT_CHAR_CFG_UUID)},
            GATT_PROP_READ | GATT_PROP_WRITE,
            2,
            NULL,
        },
    [HID_IDX_REPORT_IN_REF] =
        {
            {UUID_SIZE_2, UUID16_ARR(0x2908)}, /* Report Reference */
            GATT_PROP_READ,
            sizeof(hid_in_report_ref),
            (uint8_t*)hid_in_report_ref,
        },

    [HID_IDX_REPORT_OUT_DECL] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)},
            GATT_PROP_READ,
            0,
            NULL,
        },
    [HID_IDX_REPORT_OUT_VAL] =
        {
            {UUID_SIZE_2, UUID16_ARR(0x2A4D)}, /* Report (Output) */
            GATT_PROP_READ | GATT_PROP_WRITE,
            sizeof(hid_output_report),
            hid_output_report,
        },
    [HID_IDX_REPORT_OUT_REF] =
        {
            {UUID_SIZE_2, UUID16_ARR(0x2908)}, /* Report Reference */
            GATT_PROP_READ,
            sizeof(hid_out_report_ref),
            (uint8_t*)hid_out_report_ref,
        },
};

static void
hid_ntf_input_report(uint8_t con_idx, const uint8_t* data, uint16_t len) {
    if (hid_svc_id == 0)
        return;
    if (con_idx >= SP_MAX_CONN_NUM)
        return;
    if (hid_in_ntf_enable[con_idx] == 0)
        return;

    gatt_ntf_t ntf_att;
    ntf_att.att_idx  = HID_IDX_REPORT_IN_VAL;
    ntf_att.conidx   = con_idx;
    ntf_att.svc_id   = hid_svc_id;
    ntf_att.data_len = len;
    ntf_att.p_data   = (uint8_t*)data;
    gatt_notification(ntf_att);
}

/*********************************************************************
 * Profile Attributes - Table
 * ÿһ���һ��attribute�Ķ��塣
 * ��һ��attributeΪService �ĵĶ��塣
 * ÿһ������ֵ(characteristic)�Ķ��壬�����ٰ�������attribute�Ķ��壻
 * 1. ����ֵ����(Characteristic Declaration)
 * 2. ����ֵ��ֵ(Characteristic value)
 * 3. ����ֵ������(Characteristic description)
 * �����notification ����indication �Ĺ��ܣ��������ĸ�attribute�Ķ��壬����ǰ�涨���������������һ������ֵ�ͻ�������?(client characteristic configuration)��
 *
 */

const gatt_attribute_t simple_profile_att_table[SP_IDX_NB] = {
    // Simple gatt Service Declaration
    [SP_IDX_SERVICE] =
        {
            {UUID_SIZE_2,
             UUID16_ARR(
                 GATT_PRIMARY_SERVICE_UUID)}, /* UUID: Primary Service (0x2800) */
            GATT_PROP_READ,                   /* Permissions */
            UUID_SIZE_16, /* Max size of the value: 16 bytes for 128-bit UUID */
            (uint8_t*)sp_svc_uuid, /* Value: 128-bit Service UUID */
        },

    // Characteristic 1 Declaration
    [SP_IDX_CHAR1_DECLARATION] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)}, /* UUID */
            GATT_PROP_READ,                                 /* Permissions */
            0,    /* Max size of the value */
            NULL, /* Value of the attribute */
        },
    // Characteristic 1 Value (UUID: 0xFFF6 - 128 bit)
    // User: 0xFFF6 -> READ/WRITE/NOTIFY
    [SP_IDX_CHAR1_VALUE] =
        {
            {UUID_SIZE_16, SP_CHAR1_UUID_128}, /* UUID: 128-bit */
            GATT_PROP_READ | GATT_PROP_WRITE | GATT_PROP_NOTI, /* Permissions */
            64,   /* Max size of the value */
            NULL, /* Value of the attribute */
        },

    // Characteristic 4 client characteristic configuration
    [SP_IDX_CHAR1_CFG] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CLIENT_CHAR_CFG_UUID)}, /* UUID */
            GATT_PROP_READ | GATT_PROP_WRITE, /* Permissions */
            2,                                /* Max size of the value */
            NULL,
            /* Value of the attribute */ /* Can assign a buffer here, or can be assigned in the application by user */
        },

    // Characteristic 1 User Description
    [SP_IDX_CHAR1_USER_DESCRIPTION] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHAR_USER_DESC_UUID)}, /* UUID */
            GATT_PROP_READ,          /* Permissions */
            SP_CHAR1_DESC_LEN,       /* Max size of the value */
            (uint8_t*)sp_char1_desc, /* Value of the attribute */
        },

    // Characteristic 2 Declaration
    [SP_IDX_CHAR2_DECLARATION] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)}, /* UUID */
            GATT_PROP_READ,                                 /* Permissions */
            0,    /* Max size of the value */
            NULL, /* Value of the attribute */
        },
    // Characteristic 2 Value (UUID: 0xFFF1 - 128 bit)
    // User: 0xFFF1 -> READ/NOTIFY (Note: Picture said "Receive" but property list says Read/Notify. We follow strict property list.)
    [SP_IDX_CHAR2_VALUE] =
        {
            {UUID_SIZE_16, SP_CHAR2_UUID_128}, /* UUID: 128-bit */
            GATT_PROP_READ | GATT_PROP_NOTI,   /* Permissions */
            64,                                /* Max size of the value */
            NULL,
            /* Value of the attribute */ /* Can assign a buffer here, or can be assigned in the application by user */
        },
    // Characteristic 2 client characteristic configuration
    [SP_IDX_CHAR2_CFG] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CLIENT_CHAR_CFG_UUID)}, /* UUID */
            GATT_PROP_READ | GATT_PROP_WRITE, /* Permissions */
            2,                                /* Max size of the value */
            NULL,                             /* Value of the attribute */
        },

    // Characteristic 2 User Description
    [SP_IDX_CHAR2_USER_DESCRIPTION] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHAR_USER_DESC_UUID)}, /* UUID */
            GATT_PROP_READ,          /* Permissions */
            SP_CHAR2_DESC_LEN,       /* Max size of the value */
            (uint8_t*)sp_char2_desc, /* Value of the attribute */
        },

    // Characteristic 3 Declaration
    [SP_IDX_CHAR3_DECLARATION] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID)}, /* UUID */
            GATT_PROP_READ,                                 /* Permissions */
            0,    /* Max size of the value */
            NULL, /* Value of the attribute */
        },
    // Characteristic 3 Value (UUID: 0x1004 - 128 bit)
    // User: 0x1004 -> WRITE
    [SP_IDX_CHAR3_VALUE] =
        {
            {UUID_SIZE_16, SP_CHAR3_UUID_128}, /* UUID: 128-bit */
            GATT_PROP_WRITE,                   /* Permissions */
            64,                                /* Max size of the value */
            NULL,                              /* Value of the attribute */
        },

    // Characteristic 3 User Description
    [SP_IDX_CHAR3_USER_DESCRIPTION] =
        {
            {UUID_SIZE_2, UUID16_ARR(GATT_CHAR_USER_DESC_UUID)}, /* UUID */
            GATT_PROP_READ,          /* Permissions */
            SP_CHAR3_DESC_LEN,       /* Max size of the value */
            (uint8_t*)sp_char3_desc, /* Value of the attribute */
        },
};

static void show_reg(uint8_t* data, uint32_t len, uint8_t dbg_on) {
    uint32_t i = 0;
    if (len == 0 || (dbg_on == 0))
        return;
    for (; i < len; i++) {
        co_printf("0x%02X,", data[i]);
    }
    co_printf("\r\n");
}
void ntf_data(uint8_t con_idx, uint8_t att_idx, uint8_t* data, uint16_t len) {
    gatt_ntf_t ntf_att;
    ntf_att.att_idx  = att_idx;
    ntf_att.conidx   = con_idx;
    ntf_att.svc_id   = sp_svc_id;
    ntf_att.data_len = len;
    ntf_att.p_data   = data;
    gatt_notification(ntf_att);
}

bool sp_is_char1_ntf_enabled(uint8_t con_idx) {
    if (con_idx >= SP_MAX_CONN_NUM)
        return false;
    return ntf_char1_enable[con_idx] != 0;
}

bool sp_is_char2_ntf_enabled(uint8_t con_idx) {
    if (con_idx >= SP_MAX_CONN_NUM)
        return false;
    return ntf_char2_enable[con_idx] != 0;
}

/**
 * @brief 向所有已连接且已订阅通知的设备发送通知
 * @param att_idx  特征值索引 (SP_IDX_CHAR1_VALUE 或 SP_IDX_CHAR2_VALUE)
 * @param data     数据指针
 * @param len      数据长度
 */
void ntf_data_all(uint8_t att_idx, uint8_t* data, uint16_t len) {
    for (uint8_t idx = 0; idx < SP_MAX_CONN_NUM; idx++) {
        /* 检查连接是否存在 */
        if (gap_get_connect_status(idx) == 0) {
            continue;
        }
        /* 检查该连接是否订阅了通知 */
        if (att_idx == SP_IDX_CHAR1_VALUE && ntf_char1_enable[idx]) {
            ntf_data(idx, att_idx, data, len);
        } else if (att_idx == SP_IDX_CHAR2_VALUE && ntf_char2_enable[idx]) {
            ntf_data(idx, att_idx, data, len);
        }
    }
}
/*********************************************************************
 * @fn      sp_gatt_msg_handler
 *
 * @brief   Simple Profile callback funtion for GATT messages. GATT read/write
 *			operations are handeled here.
 *
 * @param   p_msg       - GATT messages from GATT layer.
 *
 * @return  uint16_t    - Length of handled message.
 */
static uint16_t sp_gatt_msg_handler(gatt_msg_t* p_msg) {
    switch (p_msg->msg_evt) {
    case GATTC_MSG_READ_REQ: {
        /*********************************************************************
 * @brief   Simple Profile user application handles read request in this callback.
 *			Ӧ�ò�������ص��������洦����������?
 *
 * @param   p_msg->param.msg.p_msg_data  - the pointer to read buffer. NOTE: It's just a pointer from lower layer, please create the buffer in application layer.
 *					  ָ�����������ָ��? ��ע����ֻ��һ��ָ�룬����Ӧ�ó����з��仺����. Ϊ�������?, ����?ָ���ָ��?.
 *          p_msg->param.msg.msg_len     - the pointer to the length of read buffer. Application to assign it.
 *                    ���������ĳ��ȣ��û�Ӧ�ó���ȥ������ֵ.
 *          p_msg->att_idx - index of the attribute value in it's attribute table.
 *					  Attribute��ƫ����.
 *
 * @return  ������ĳ���?.
 */

        if (p_msg->att_idx == SP_IDX_CHAR1_VALUE) {
            /*
                         * 为何返回 0 长度：
                         * - nRF Connect/手机在发现服务后常会主动 READ 一次特征值，导致界面“刚连接就有 Value”。
                         * - 之前这里返回固定字符串 "CHAR1_VALUE" 只是占位，会误导联调。
                         * - 这里改为返回空，让该特征的值仅在 Notify/业务协议交互时体现。
                         */
            return 0;
        } else if (p_msg->att_idx == SP_IDX_CHAR2_VALUE) {
            /* 同 CHAR1：READ 返回空，避免显示占位字符串 */
            return 0;
        }

    } break;

    case GATTC_MSG_WRITE_REQ: {
        /*********************************************************************
 * @brief   Simple Profile user application handles write request in this callback.
 *			Ӧ�ò�������ص��������洦��д������?
 *
 * @param   p_msg->param.msg.p_msg_data   - the buffer for write
 *			              д����������.
 *					  
 *          p_msg->param.msg.msg_len      - the length of write buffer.
 *                        д�������ĳ���.
 *          att_idx     - index of the attribute value in it's attribute table.
 *					      Attribute��ƫ����.
 *
 * @return  д����ĳ���?.
 */
        if (p_msg->att_idx == SP_IDX_CHAR1_VALUE) {
            co_printf("char1_recv:");
            show_reg(p_msg->param.msg.p_msg_data, p_msg->param.msg.msg_len, 1);
            co_printf("  [rx] att_idx=%d len=%d\r\n",
                      p_msg->att_idx,
                      p_msg->param.msg.msg_len);

            /* 记录本次收包来自哪个通道，回包将跟随此通道（CHAR1/CHAR2） */
            Protocol_Set_Rx_AttIdx(p_msg->conn_idx, p_msg->att_idx);

            // Pass data to protocol handler (带上连接索引用于 ACK 处理)
            Protocol_Handle_Data(p_msg->conn_idx,
                                 p_msg->param.msg.p_msg_data,
                                 p_msg->param.msg.msg_len);
        } else if (p_msg->att_idx == SP_IDX_CHAR2_VALUE) {
            co_printf("char2_recv:");
            show_reg(p_msg->param.msg.p_msg_data, p_msg->param.msg.msg_len, 1);
            co_printf("  [rx] att_idx=%d len=%d\r\n",
                      p_msg->att_idx,
                      p_msg->param.msg.msg_len);

            /* 记录本次收包来自哪个通道，回包将跟随此通道（CHAR1/CHAR2） */
            Protocol_Set_Rx_AttIdx(p_msg->conn_idx, p_msg->att_idx);

            /* 兼容：若 APP 写到了 CHAR2，也尝试按协议解析 */
            Protocol_Handle_Data(p_msg->conn_idx,
                                 p_msg->param.msg.p_msg_data,
                                 p_msg->param.msg.msg_len);
        } else if (p_msg->att_idx == SP_IDX_CHAR3_VALUE) {
            co_printf("char3_recv:");
            show_reg(p_msg->param.msg.p_msg_data, p_msg->param.msg.msg_len, 1);
            co_printf("  [rx] att_idx=%d len=%d\r\n",
                      p_msg->att_idx,
                      p_msg->param.msg.msg_len);

            /* CHAR3 仅写入无 Notify：记录后发送侧会回退到可通知通道 */
            Protocol_Set_Rx_AttIdx(p_msg->conn_idx, p_msg->att_idx);

            /* 兼容：若 APP 写到了 CHAR3，也尝试按协议解析 */
            Protocol_Handle_Data(p_msg->conn_idx,
                                 p_msg->param.msg.p_msg_data,
                                 p_msg->param.msg.msg_len);
        } else if (p_msg->att_idx == SP_IDX_CHAR1_CFG) {
            co_printf("char1_ntf_enable[%d]:", p_msg->conn_idx);
            show_reg(p_msg->param.msg.p_msg_data, p_msg->param.msg.msg_len, 1);
            if (p_msg->conn_idx < SP_MAX_CONN_NUM) {
                ntf_char1_enable[p_msg->conn_idx] =
                    (p_msg->param.msg.p_msg_data[0] & 0x1) ? 1 : 0;
            }
        } else if (p_msg->att_idx == SP_IDX_CHAR2_CFG) {
            co_printf("char2_ntf_enable[%d]:", p_msg->conn_idx);
            show_reg(p_msg->param.msg.p_msg_data, p_msg->param.msg.msg_len, 1);
            if (p_msg->conn_idx < SP_MAX_CONN_NUM) {
                ntf_char2_enable[p_msg->conn_idx] =
                    (p_msg->param.msg.p_msg_data[0] & 0x1) ? 1 : 0;
            }
        }
    } break;
    case GATTC_MSG_LINK_CREATE: co_printf("link_created\r\n"); break;
    case GATTC_MSG_LINK_LOST:
        co_printf("link_lost[%d]\r\n", p_msg->conn_idx);
        if (p_msg->conn_idx < SP_MAX_CONN_NUM) {
            ntf_char1_enable[p_msg->conn_idx] = 0;
            ntf_char2_enable[p_msg->conn_idx] = 0;
        }

        /* 复位默认回包通道 */
        Protocol_Set_Rx_AttIdx(p_msg->conn_idx, SP_IDX_CHAR1_VALUE);
        break;
    default: break;
    }
    return p_msg->param.msg.msg_len;
}

static uint16_t hid_gatt_msg_handler(gatt_msg_t* p_msg) {
    switch (p_msg->msg_evt) {
    case GATTC_MSG_READ_REQ: {
        if (p_msg->att_idx == HID_IDX_PROTOCOL_MODE_VAL) {
            p_msg->param.msg.p_msg_data[0] = hid_protocol_mode;
            return 1;
        } else if (p_msg->att_idx == HID_IDX_REPORT_IN_VAL) {
            /* return last input report */
            uint16_t copy_len =
                (p_msg->param.msg.msg_len < sizeof(hid_input_report))
                    ? p_msg->param.msg.msg_len
                    : sizeof(hid_input_report);
            memcpy(p_msg->param.msg.p_msg_data, hid_input_report, copy_len);
            return copy_len;
        } else if (p_msg->att_idx == HID_IDX_REPORT_OUT_VAL) {
            uint16_t copy_len =
                (p_msg->param.msg.msg_len < sizeof(hid_output_report))
                    ? p_msg->param.msg.msg_len
                    : sizeof(hid_output_report);
            memcpy(p_msg->param.msg.p_msg_data, hid_output_report, copy_len);
            return copy_len;
        }
    } break;

    case GATTC_MSG_WRITE_REQ: {
        if (p_msg->att_idx == HID_IDX_PROTOCOL_MODE_VAL) {
            if (p_msg->param.msg.msg_len >= 1) {
                hid_protocol_mode = p_msg->param.msg.p_msg_data[0];
            }
        } else if (p_msg->att_idx == HID_IDX_CONTROL_POINT_VAL) {
            /* 0=suspend, 1=exit suspend; ignore for now */
        } else if (p_msg->att_idx == HID_IDX_REPORT_IN_CFG) {
            if (p_msg->conn_idx < SP_MAX_CONN_NUM) {
                hid_in_ntf_enable[p_msg->conn_idx] =
                    (p_msg->param.msg.p_msg_data[0] & 0x1) ? 1 : 0;
                co_printf("hid_in_ntf_enable[%d]=%d\r\n",
                          p_msg->conn_idx,
                          hid_in_ntf_enable[p_msg->conn_idx]);
            }
        } else if (p_msg->att_idx == HID_IDX_REPORT_OUT_VAL) {
            /* Host writes LED state: [ID][LEDs] (with Report ID=1) */
            uint16_t copy_len =
                (p_msg->param.msg.msg_len < sizeof(hid_output_report))
                    ? p_msg->param.msg.msg_len
                    : sizeof(hid_output_report);
            memcpy(hid_output_report, p_msg->param.msg.p_msg_data, copy_len);
        }
    } break;

    case GATTC_MSG_LINK_LOST: {
        if (p_msg->conn_idx < SP_MAX_CONN_NUM) {
            hid_in_ntf_enable[p_msg->conn_idx] = 0;
        }
    } break;

    default: break;
    }
    return p_msg->param.msg.msg_len;
}

/*********************************************************************
 * @fn      sp_gatt_add_service
 *
 * @brief   Simple Profile add GATT service function.
 *			����GATT service��ATT�����ݿ����档
 *
 * @param   None. 
 *        
 *
 * @return  None.
 */
void sp_gatt_add_service(void) {
    simple_profile_svc.p_att_tb         = simple_profile_att_table;
    simple_profile_svc.att_nb           = SP_IDX_NB;
    simple_profile_svc.gatt_msg_handler = sp_gatt_msg_handler;

    sp_svc_id = gatt_add_service(&simple_profile_svc);

    /* Add HID service (HOGP) for universal auto-reconnect behavior after bonding */
    hid_profile_svc.p_att_tb         = hid_profile_att_table;
    hid_profile_svc.att_nb           = HID_IDX_NB;
    hid_profile_svc.gatt_msg_handler = hid_gatt_msg_handler;
    hid_svc_id                       = gatt_add_service(&hid_profile_svc);
}
