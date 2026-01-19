#include <stdint.h>

#include "os_task.h"
#include "os_msg_q.h"

#include "co_printf.h"
#include "user_task.h"
#include "button.h"
#include "TPMS.h"
#include "usart_cmd.h"

#include "usart_device.h" 
#include "TPMS.h" // Add TPMS header

/* MCU 回包判定需要把 UART 帧上报给 ble_function */
#include "ble_function.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

uint16_t user_task_id;

static void trim_line(char *s)
{
    if (s == NULL) return;
    /* 去掉 \r\n */
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[n - 1] = '\0';
        n--;
    }
}

static int str_ieq(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = (char)tolower((unsigned char)*a++);
        char cb = (char)tolower((unsigned char)*b++);
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static char *next_token(char **p)
{
    char *s = (p && *p) ? *p : NULL;
    if (!s) return NULL;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') {
        *p = s;
        return NULL;
    }
    char *tok = s;
    while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') s++;
    if (*s) {
        *s++ = '\0';
    }
    *p = s;
    return tok;
}

static void handle_at_command(char *line)
{
    trim_line(line);
    char *p = line;
    char *t0 = next_token(&p);
    if (!t0) return;

    if (!str_ieq(t0, "TPMS")) {
        return;
    }

    char *cmd = next_token(&p);
    if (!cmd) {
        co_printf("[TPMS] usage: TPMS SHOW | TPMS REPLACE <wheel_idx> [window_ms] | TPMS CLEAR <wheel_idx|ALL>\r\n");
        return;
    }

    if (str_ieq(cmd, "SHOW")) {
        TPMS_Binding_Print();
        TPMS_Debug_Print();
        return;
    }
    if (str_ieq(cmd, "REPLACE")) {
        char *w = next_token(&p);
        char *ms = next_token(&p);
        if (!w) {
            co_printf("[TPMS] REPLACE need wheel_idx\r\n");
            return;
        }
        uint8_t wheel = (uint8_t)atoi(w);
        uint32_t window_ms = ms ? (uint32_t)atoi(ms) : 20000u;
        TPMS_Replace_Start(wheel, window_ms);
        return;
    }
    if (str_ieq(cmd, "CLEAR")) {
        char *w = next_token(&p);
        if (!w) {
            co_printf("[TPMS] CLEAR need wheel_idx or ALL\r\n");
            return;
        }
        if (str_ieq(w, "ALL")) {
            TPMS_Clear_All_Bindings();
            return;
        }
        TPMS_Clear_Binding((uint8_t)atoi(w));
        return;
    }

    co_printf("[TPMS] unknown cmd: %s\r\n", cmd);
}

typedef struct {
    uint16_t len;
    uint8_t data[1];
} uart_frame_msg_t;

static uint8_t BinComplementCheckSum(const uint8_t *buf, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + buf[i]);
    }
    return (uint8_t)(~sum + 1u);
}

static void handle_uart_frame(const uart_frame_msg_t *m)
{
    if (!m || m->len < 1u) return;
    const uint8_t *f = m->data;
    const uint16_t flen = m->len;

    /* 基本长度校验：head(1)+sync(1)+feature(2)+id(2)+len(2)+crc+end(2) */
    const uint16_t min_len = (uint16_t)(1u + 1u + 2u + 2u + 2u + SOC_MCU_CRC_LEN + 2u);
    if (flen < min_len) {
        co_printf("[UART] frame too short: %u\r\n", (unsigned)flen);
        return;
    }
    if (f[0] != SOC_MCU_FRAME_HEAD) {
        co_printf("[UART] bad head: 0x%02X\r\n", (unsigned)f[0]);
        return;
    }
    if (f[flen - 2] != SOC_MCU_FRAME_END0 || f[flen - 1] != SOC_MCU_FRAME_END1) {
        co_printf("[UART] bad end: %02X %02X\r\n", (unsigned)f[flen - 2], (unsigned)f[flen - 1]);
        return;
    }

    /* Big Endian Parsing */
    uint16_t sync = (uint16_t)f[1]; /* 1 Byte Sync */
    uint16_t feature = (uint16_t)((f[2] << 8) | f[3]);
    uint16_t id = (uint16_t)((f[4] << 8) | f[5]);
    uint16_t len_field = (uint16_t)((f[6] << 8) | f[7]);
    uint16_t data_len = len_field;
    /* total = head(1)+sync(1)+feature(2)+id(2)+len(2)+data+crc+end(2) */
    uint16_t expected = (uint16_t)(1u + 1u + 2u + 2u + 2u + data_len + SOC_MCU_CRC_LEN + 2u);
    if (expected != flen) {
        co_printf("[UART] len mismatch: expected=%u actual=%u\r\n", (unsigned)expected, (unsigned)flen);
        return;
    }

    if (sync != (SOC_MCU_SYNC_SOC_TO_MCU & 0xFF) && sync != (SOC_MCU_SYNC_MCU_TO_SOC & 0xFF)) {
        co_printf("[UART] bad sync: 0x%02X\r\n", (unsigned)sync);
        return;
    }

    const uint8_t *payload = &f[8];
    const uint8_t *crc_ptr = &payload[data_len];

    uint8_t crc_rx = crc_ptr[0];
    uint16_t crc_off = (uint16_t)(8u + data_len);
    uint8_t crc_calc = BinComplementCheckSum(f, crc_off);
    bool crc_ok = (crc_calc == crc_rx);

    co_printf("[UART] sync=0x%04X feature=0x%04X id=0x%04X data_len=%u crc_%s\r\n",
              (unsigned)sync,
              (unsigned)feature,
              (unsigned)id,
              (unsigned)data_len,
              crc_ok ? "OK" : "BAD");

    if (crc_ok && id == CMD_Tire_pressure_monitoring_get && 
        feature == SOC_MCU_FEATURE_FF01 && sync == (SOC_MCU_SYNC_SOC_TO_MCU & 0xFF)) {
        /* 特殊处理: MCU 主动请求获取 TPMS 数据 (0x117) via UART */
        /* Payload: [0] = Wheel Index */
        if (data_len >= 1) {
            uint8_t wheel = payload[0];
            co_printf("[UART] TPMS Get Req wheel=%d\r\n", wheel);
            
            TPMS_Frame frame;
            /* Resp payload: Wheel(1)+ID(4)+Press(2)+Temp(1)+Volt(2)+Status(1)+MAC(6) = 17 Bytes */
            uint8_t resp_data[17];
            memset(resp_data, 0, sizeof(resp_data));
            
            resp_data[0] = wheel; // echo wheel index
            
            if (TPMS_Get_Sensor(wheel, &frame)) {
                /* All Multi-Byte Fields refer to Big Endian */
                
                // Sensor ID (4B)
                resp_data[1] = (uint8_t)((frame.sensor_id >> 24) & 0xFF);
                resp_data[2] = (uint8_t)((frame.sensor_id >> 16) & 0xFF);
                resp_data[3] = (uint8_t)((frame.sensor_id >> 8) & 0xFF);
                resp_data[4] = (uint8_t)(frame.sensor_id & 0xFF);

                // Pressure (2B)
                resp_data[5] = (uint8_t)((frame.pressure_kpa_x100 >> 8) & 0xFF);
                resp_data[6] = (uint8_t)(frame.pressure_kpa_x100 & 0xFF);

                // Temp (1B)
                resp_data[7] = (uint8_t)frame.temperature_c;

                // Voltage (2B)
                resp_data[8] = (uint8_t)((frame.battery_v_x100 >> 8) & 0xFF);
                resp_data[9] = (uint8_t)(frame.battery_v_x100 & 0xFF);
                
                // Status (1B)
                resp_data[10] = frame.status_raw;
                
                // MAC (6B)
                memcpy(&resp_data[11], frame.bt_addr, 6);
            } else {
                 co_printf("[UART] TPMS Get Req wheel=%d No Data\r\n", wheel);
            }
            
            // Send Response: Sync=BA, Feature=FF02 (Resp), ID=0x117
            SocMcu_Frame_Send(SOC_MCU_SYNC_MCU_TO_SOC, /* 0xBA */
                              SOC_MCU_FEATURE_FF02,    /* Resp */
                              CMD_Tire_pressure_monitoring_get,
                              resp_data,
                              sizeof(resp_data));
            return; // Handled, no need to pass to BleFunc
        }
    }

    /* 上报�?BLE 业务层：用于判断 MCU 是否回包 */
    BleFunc_OnMcuUartFrame(sync, feature, id, payload, data_len, (uint8_t)(crc_ok ? 1 : 0));
}

static int user_task_func(os_event_t *param)
{
    switch(param->event_id)
    {
        case USER_EVT_AT_COMMAND:
            {
                /* os_msg_malloc() 返回�?payload 即为字符�?*/
                char *line = (char *)param->param;
                if (line) {
                    handle_at_command(line);
                }
            }
            break;
        case USER_EVT_UART_FRAME:
            {
                const uart_frame_msg_t *m = (const uart_frame_msg_t *)param->param;
                if (m) {
                    handle_uart_frame(m);
                }
            }
            break;
        case USER_EVT_BUTTON:
            {
                struct button_msg_t *button_msg;
                const char *button_type_str[] = {
                                                    "BUTTON_PRESSED",
                                                    "BUTTON_RELEASED",
                                                    "BUTTON_SHORT_PRESSED",
                                                    "BUTTON_MULTI_PRESSED",
                                                    "BUTTON_LONG_PRESSED",
                                                    "BUTTON_LONG_PRESSING",
                                                    "BUTTON_LONG_RELEASED",
                                                    "BUTTON_LONG_LONG_PRESSED",
                                                    "BUTTON_LONG_LONG_RELEASED",
                                                    "BUTTON_COMB_PRESSED",
                                                    "BUTTON_COMB_RELEASED",
                                                    "BUTTON_COMB_SHORT_PRESSED",
                                                    "BUTTON_COMB_LONG_PRESSED",
                                                    "BUTTON_COMB_LONG_PRESSING",
                                                    "BUTTON_COMB_LONG_RELEASED",
                                                    "BUTTON_COMB_LONG_LONG_PRESSED",
                                                    "BUTTON_COMB_LONG_LONG_RELEASED",
                                                };

                button_msg = (struct button_msg_t *)param->param;
                
                co_printf("KEY 0x%08x, TYPE %s.\r\n", button_msg->button_index, button_type_str[button_msg->button_type]);
            }
            break;
    }

    return EVT_CONSUMED;
}

void user_task_init(void)
{
    user_task_id = os_task_create(user_task_func);
}



