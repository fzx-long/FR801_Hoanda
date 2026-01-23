#include "protocol.h"
#include "protocol_cmd.h"
#include "protocol_fe.h"
#include "protocol_fd.h"
#include "phone_reply.h"
#include "simple_gatt_service.h"
#include "gap_api.h"
#include "os_timer.h"
#include "rssi_check.h"
#include "param_sync.h"
#include "en_de_algo.h" // 引入加密算法库
#include <string.h>
#include "co_printf.h"

// 定义预置密钥（AES-128 Key，16 字节 ASCII）
// 注意：这里必须与手机端 AES 加解密使用的 Key 完全一致。
static const uint8_t s_default_aes_key[16] = {'Q',
                                              'S',
                                              'D',
                                              'f',
                                              'a',
                                              'g',
                                              'Q',
                                              '1',
                                              '4',
                                              '1',
                                              'G',
                                              'S',
                                              '6',
                                              'J',
                                              'F',
                                              '8'};
static const uint8_t s_default_aes_iv[16]  = {0};

// 全局协议处理对象
Protocol_Handler_t g_protocol_handler;

/* 当前正在解析的连接索引（为业务层提供 conidx 上下文） */
static uint8_t g_protocol_rx_conidx = 0xFF;

/* 发送上下文（按连接跟踪 ACK） */
#define PROTOCOL_MAX_CONN 3 /* 与 simple_gatt_service 的 SP_MAX_CONN_NUM 对齐 */
#define PROTOCOL_MAX_LEN                                                       \
    240 /* payload 最大长度，确保总长度 fits uint8_t length 字段 */
#define PROTOCOL_ACK_TIMEOUT 3000
#define PROTOCOL_MAX_RETRY   3

/*
 * 发送调试开关（为什么需要）：
 * - 你当前日志里已经看到 0x01FE 被正确解密/鉴权成功，但 App 没看到 0x0101。
 * - 0x0101 是通过 GATT Notify 发出，若 App 未订阅正确的特征（CHAR1/CHAR2），或者设备选错通道，就会“看起来没回包”。
 * - 打开该日志能直接看到：本次发送选了哪个 att_idx、cmd/len、以及帧头 16B。
 */
#ifndef PROTOCOL_DEBUG_TX
#define PROTOCOL_DEBUG_TX 1
#endif

/*
 * 回包通道固定开关（为什么需要）：
 * - 目前发送通道会随“最近一次 RX 的 att_idx + notify 使能”在两个特征之间切换，App 若只订阅一个特征会出现“偶发收不到回包”。
 * - 你希望“统一用 FFF1 进行回复”。在当前工程的 GATT 表里：
 *   - UUID=FFF6 对应 SP_IDX_CHAR1_VALUE（可写，通常用于手机->设备发送）
 *   - UUID=FFF1 对应 SP_IDX_CHAR2_VALUE（只读/notify，通常用于设备->手机回复）
 * - 因此这里提供固定：所有协议帧一律从 SP_IDX_CHAR2_VALUE(FFF1) notify。
 */
#ifndef PROTOCOL_TX_FORCE_FFF1
#define PROTOCOL_TX_FORCE_FFF1 1
#endif

/* 鉴权状态（按连接） */
static uint8_t g_protocol_authed[PROTOCOL_MAX_CONN] = {0};

/* 每个连接：最后一次从哪个特征值收到数据（用于回包跟随通道） */
static uint8_t g_protocol_last_rx_att_idx[PROTOCOL_MAX_CONN] = {0};

/*
 * 每个连接：最后一次收到的“流水号(seq)”（用于同步应答回显）
 * 协议要求：终端回复的流水号与 APP 下发的流水号一致。
 */
static uint8_t g_protocol_last_rx_seq[PROTOCOL_MAX_CONN] = {0xFF, 0xFF, 0xFF};

/* 每个连接：最后一次收到的加密类型（用于回包按同样方式加密） */
static uint8_t g_protocol_last_rx_crypto[PROTOCOL_MAX_CONN] = {
    CRYPTO_TYPE_NONE, CRYPTO_TYPE_NONE, CRYPTO_TYPE_NONE};

/* gap_api.h 在当前 SDK 版本中若未暴露原型，这里显式声明以避免隐式声明告警 */
void gap_disconnect_req(uint8_t conidx);

#if PROTOCOL_USE_ACK
typedef struct
{
    bool     in_flight;
    bool     critical;
    uint8_t  seq;
    uint16_t cmd;
    uint8_t  retry;
    uint8_t  buf[PROTOCOL_MAX_LEN + 10];
    uint16_t len;
} proto_tx_ctx_t;

static proto_tx_ctx_t g_tx_ctx[PROTOCOL_MAX_CONN];
static os_timer_t     g_ack_timer[PROTOCOL_MAX_CONN];
#endif

static uint8_t g_seq = 0;

/*
 * [稳定性] 降低栈占用：
 * - 之前 Protocol_Send_Unicast/Async/Broadcast 在栈上分配 frame/enc_payload 大数组。
 * - 这些函数经常在 BLE 协议栈回调、UART 任务回调中被调用，任务/回调栈通常较小，
 *   容易触发栈溢出，表现为：PC/LR 异常（跳到 rodata/errno 等地址）=> HardFault => SOC 重启。
 * - 这里改为“按连接的静态缓冲”，避免栈爆。
 * 注意：该缓冲不是可重入的；但当前工程发送路径是串行的（同一 conidx 同时只会走一次发送）。
 */
static uint8_t s_tx_frame_buf[PROTOCOL_MAX_CONN][PROTOCOL_MAX_LEN + 10];
static uint8_t s_tx_enc_buf[PROTOCOL_MAX_CONN][PROTOCOL_MAX_LEN];

#if PROTOCOL_USE_ACK
static void proto_ack_timeout(void* arg);
static void proto_send_ack(uint8_t conidx, uint8_t seq);
static void proto_restart_timer(uint8_t conidx);
#endif
static bool
proto_send_frame(uint8_t conidx, const uint8_t* frame, uint16_t len);

// BCC 校验函数实现
static bool Protocol_Check_BCC(Protocol_Handler_t* self)
{
    if (self == NULL || self->rx_buffer == NULL || self->rx_len < 10)
    {
        return false;
    }

    uint8_t bcc = 0;
    // BCC 校验范围：从 Header 到 Data (不包含 BCC 和 Footer)
    // Header(2) + Length(1) + Crypto(1) + Seq(1) + Cmd(2) = 7 bytes
    // Data 长度 = self->payload_len
    // 总校验长度 = 7 + self->payload_len
    // 也就是 rx_len - 3 (BCC 1 byte + Footer 2 bytes)

    uint16_t check_len = self->rx_len - 3;

    for (uint16_t i = 0; i < check_len; i++)
    {
        bcc ^= self->rx_buffer[i];
    }

    // 获取接收到的 BCC (倒数第3个字节)
    uint8_t received_bcc = self->rx_buffer[self->rx_len - 3];

    return (bcc == received_bcc);
}

// 协议解析函数实现
static bool Protocol_Parse(Protocol_Handler_t* self)
{
    if (self == NULL || self->rx_buffer == NULL)
    {
        return false;
    }

    // 1. 检查最小长度 (Header 2 + Len 1 + Crypto 1 + Seq 1 + Cmd 2 + BCC 1 + Footer 2 = 10)
    if (self->rx_len < 10)
    {
        co_printf("Protocol: Len too short %d\r\n", self->rx_len);
        return false;
    }

    // 2. 映射头部
    Protocol_Header_t* pHead = (Protocol_Header_t*)self->rx_buffer;

    // 3. 检查 Header Magic (0x5555)
    if (pHead->header != PROTOCOL_HEADER_MAGIC)
    {
        co_printf("Protocol: Header Error 0x%04X\r\n", pHead->header);
        return false;
    }

    // 4. 检查 Footer Magic (0xAAAA)
    // Footer 位于最后两个字节
    if (self->rx_buffer[self->rx_len - 2] != 0xAA ||
        self->rx_buffer[self->rx_len - 1] != 0xAA)
    {
        co_printf("Protocol: Footer Error\r\n");
        return false;
    }
    // 5. 检查长度字段
    // 协议定义 Length 为 "数据包长度，包括 header、footer"
    if (pHead->length != self->rx_len)
    {
        co_printf(
            "Protocol: Len mismatch %d != %d\r\n", pHead->length, self->rx_len);
        return false;
    }

    // 6. 填充 header_info
    self->header_info = *pHead;

    /* [Fix] 协议兼容：App 发送的 Cmd 为大端序 (如 01 FE)，ARM 小端读取为 0xFE01
     * 需转换为本地小端序 (0x01FE) 以匹配 protocol_cmd.h 中的定义
     */
    self->header_info.cmd =
        (uint16_t)((self->header_info.cmd >> 8) | (self->header_info.cmd << 8));

    // 7. 计算 Payload 信息
    // Payload 位于 Cmd 之后，BCC 之前
    // Header(2)+Len(1)+Crypto(1)+Seq(1)+Cmd(2) = 7 bytes offset
    self->payload     = &self->rx_buffer[7];
    self->payload_len = self->rx_len - 10; // 总长 - (Header7 + BCC1 + Footer2)

    // 8. 校验 BCC
    // 先校验完整性，再进行解密
    if (!self->Check_BCC(self))
    {
        co_printf("Protocol: BCC Error\r\n");
        return false;
    }

    // 9. 解密 Data 段 (OPP 动态策略)
    if (self->payload_len > 0)
    {
        Algo_Context_t algo_ctx;

        // A. 动态绑定算法 (0x00=None, 0x01=DES3, 0x02=AES128)
        if (Algo_Bind(&algo_ctx, (algo_type_t)pHead->crypto))
        {

            /* Debug：打印解密前 payload 前 16 字节，便于对齐 App 端 AES 参数 */
            co_printf("Protocol: crypto=0x%02X payload_len=%d\r\n",
                      (unsigned)pHead->crypto,
                      (int)self->payload_len);
            co_printf("Protocol: payload head (enc): ");
            for (uint16_t i = 0; i < 16 && i < self->payload_len; i++)
            {
                co_printf("%02X ", self->payload[i]);
            }
            co_printf("\r\n");

            // B. 设置密钥 (仅当非 None 模式时生效)
            if (pHead->crypto != ALGO_TYPE_NONE)
            {
                Algo_SetKeyIV(&algo_ctx, s_default_aes_key, s_default_aes_iv);
            }

            // C. 执行解密 (原地解密: 输入=输出=payload)
            if (!Algo_Decrypt(
                    &algo_ctx, self->payload, self->payload_len, self->payload))
            {
                co_printf("Protocol: Decrypt Error\r\n");
                return false;
            }

            /*
             * PKCS7 去填充（为什么需要）：
             * - App 端 AES 使用 PKCS7Padding（你提供的工具截图）。
             * - 例如 Connect(0x01FE) 明文应为 39 字节，但加密后会填充到 48 字节。
             * - 若不去填充，业务层会把 padding 当成 token/mobileSystem，导致 time_bcd 解析失败与鉴权失败。
             */
            if (pHead->crypto == ALGO_TYPE_AES_CBC && self->payload_len >= 16)
            {
                uint8_t pad = self->payload[self->payload_len - 1];
                if (pad >= 1 && pad <= 16 && pad <= self->payload_len)
                {
                    bool pad_ok = true;
                    for (uint16_t i = 0; i < pad; i++)
                    {
                        if (self->payload[self->payload_len - 1 - i] != pad)
                        {
                            pad_ok = false;
                            break;
                        }
                    }
                    if (pad_ok)
                    {
                        self->payload_len = (uint16_t)(self->payload_len - pad);
                        co_printf("Protocol: unpad pkcs7 -> payload_len=%d\r\n",
                                  (int)self->payload_len);
                    }
                }
            }

            /* Debug：打印解密后 payload 前 16 字节 */
            co_printf("Protocol: payload head (dec): ");
            for (uint16_t i = 0; i < 16 && i < self->payload_len; i++)
            {
                co_printf("%02X ", self->payload[i]);
            }
            co_printf("\r\n");
        }
        else
        {
            co_printf("Protocol: Unknown Algo 0x%02X\r\n", pHead->crypto);
            return false;
        }
    }

    return true;
}

// 初始化协议处理器
void Protocol_Init(void)
{
    g_protocol_handler.rx_buffer = NULL;
    g_protocol_handler.rx_len    = 0;
    g_protocol_handler.Check_BCC = Protocol_Check_BCC;
    g_protocol_handler.Parse     = Protocol_Parse;

    for (uint8_t i = 0; i < PROTOCOL_MAX_CONN; i++)
    {
        g_protocol_last_rx_att_idx[i] = SP_IDX_CHAR1_VALUE;
        g_protocol_last_rx_seq[i]     = 0xFF;
        g_protocol_last_rx_crypto[i]  = CRYPTO_TYPE_NONE;
    }

#if PROTOCOL_USE_ACK
    memset(g_tx_ctx, 0, sizeof(g_tx_ctx));
    for (uint8_t i = 0; i < PROTOCOL_MAX_CONN; i++)
    {
        os_timer_init(&g_ack_timer[i], proto_ack_timeout, (void*)(uint32_t)i);
    }
#endif
}

// 处理接收到的数据 (供外部调用)
void Protocol_Handle_Data(uint8_t conidx, uint8_t* data, uint16_t len)
{
    g_protocol_rx_conidx         = conidx;
    g_protocol_handler.rx_buffer = data;
    g_protocol_handler.rx_len    = len;

    if (!g_protocol_handler.Parse(&g_protocol_handler))
    {
        return;
    }

    uint16_t cmd = g_protocol_handler.header_info.cmd;
    uint8_t  seq = g_protocol_handler.header_info.seq_num;

    /* 记录最近一次 RX 的流水号，用于后续回复帧“回显相同流水号” */
    if (conidx < PROTOCOL_MAX_CONN)
    {
        g_protocol_last_rx_seq[conidx] = seq;
        g_protocol_last_rx_crypto[conidx] =
            g_protocol_handler.header_info.crypto;
    }

#if PROTOCOL_USE_ACK
    /* 如果是 ACK，匹配流水号后停止对应定时器 */
    if (cmd == CMD_ACK_ID)
    {
        if (conidx < PROTOCOL_MAX_CONN && g_tx_ctx[conidx].in_flight &&
            g_tx_ctx[conidx].seq == seq)
        {
            g_tx_ctx[conidx].in_flight = false;
            os_timer_stop(&g_ack_timer[conidx]);
            co_printf("Protocol: ACK ok conidx=%d seq=%d\r\n", conidx, seq);
        }
        return;
    }

    co_printf("Protocol: Parse Success! Cmd: 0x%04X seq=%d\r\n", cmd, seq);

    /* 收到业务数据后立即回 ACK（Cmd=0x0000，Data 长度=0，加密=0x00，流水号同对端） */
    proto_send_ack(conidx, seq);
#else
    co_printf("Protocol: Parse Success! Cmd: 0x%04X seq=%d\r\n", cmd, seq);
#endif

    // 根据命令低字节区分 FE 和 FD
    uint8_t cmd_type = (uint8_t)(cmd & 0xFF);

    if (cmd_type == 0xFE)
    {
        Protocol_Process_FE(
            cmd, g_protocol_handler.payload, g_protocol_handler.payload_len);
    }
    else if (cmd_type == 0xFD)
    {
        Protocol_Process_FD(
            cmd, g_protocol_handler.payload, g_protocol_handler.payload_len);
    }
    else if (cmd_type == 0x02)
    {
        /* 兼容：设备主动推送类命令（如 0x64FD/0x66FD）对应的 APP 回复（0x6402/0x6602） */
        ParamSync_OnAppReply(conidx,
                             cmd,
                             g_protocol_handler.payload,
                             g_protocol_handler.payload_len);
    }
    else
    {
        co_printf("Protocol: Unknown Cmd Type 0x%02X\r\n", cmd_type);
    }
}

/**
 * @brief 发送侧加密封装（与接收侧解密策略一致）
 * @note 为什么要做：App 发来 crypto=0x02 的包时，通常也期望设备回包带同样 crypto，并对 Data 段做 AES+PKCS7。
 * @param crypto     加密类型（CRYPTO_TYPE_*）
 * @param plain      明文 payload
 * @param plain_len  明文长度
 * @param out        输出缓冲区（用于存放密文）
 * @param out_cap    输出缓冲区容量
 * @param out_len    输出密文长度
 */
static bool proto_encrypt_payload(uint8_t        crypto,
                                  const uint8_t* plain,
                                  uint16_t       plain_len,
                                  uint8_t*       out,
                                  uint16_t       out_cap,
                                  uint16_t*      out_len)
{
    if (out_len == NULL)
    {
        return false;
    }
    *out_len = 0;

    if (crypto == CRYPTO_TYPE_NONE || plain_len == 0)
    {
        /* 无加密：直接输出原文 */
        if (plain_len > 0)
        {
            if (plain == NULL || out == NULL || out_cap < plain_len)
            {
                return false;
            }
            memcpy(out, plain, plain_len);
        }
        *out_len = plain_len;
        return true;
    }

    /* 需要加密：必须有输入输出缓冲区 */
    if (plain == NULL || out == NULL)
    {
        return false;
    }

    Algo_Context_t algo_ctx;
    if (!Algo_Bind(&algo_ctx, (algo_type_t)crypto))
    {
        return false;
    }
    Algo_SetKeyIV(&algo_ctx, s_default_aes_key, s_default_aes_iv);

    /* PKCS7Padding：先拷贝明文到 out，再做 padding，再原地加密 */
    if (out_cap < plain_len)
    {
        return false;
    }
    memcpy(out, plain, plain_len);

    uint8_t block_size = 16u;
    if (algo_ctx.ops != NULL && algo_ctx.ops->block_size != 0)
    {
        block_size = algo_ctx.ops->block_size;
    }
    uint32_t padded_len = Algo_Padding(out, (uint32_t)plain_len, block_size);
    if (padded_len == 0 || padded_len > out_cap || padded_len > 0xFFu)
    {
        return false;
    }

    if (!Algo_Encrypt(&algo_ctx, out, padded_len, out))
    {
        return false;
    }

    *out_len = (uint16_t)padded_len;
    return true;
}

uint8_t Protocol_Get_Rx_Conidx(void)
{
    return g_protocol_rx_conidx;
}

void Protocol_Set_Rx_AttIdx(uint8_t conidx, uint16_t att_idx)
{
    if (conidx >= PROTOCOL_MAX_CONN)
        return;

    /* 只接受我们关心的通道；CHAR3 无 Notify，因此记录为 CHAR2 作为回包通道 */
    if (att_idx == SP_IDX_CHAR1_VALUE)
    {
        g_protocol_last_rx_att_idx[conidx] = SP_IDX_CHAR1_VALUE;
    }
    else if (att_idx == SP_IDX_CHAR2_VALUE)
    {
        g_protocol_last_rx_att_idx[conidx] = SP_IDX_CHAR2_VALUE;
    }
    else if (att_idx == SP_IDX_CHAR3_VALUE)
    {
        g_protocol_last_rx_att_idx[conidx] = SP_IDX_CHAR2_VALUE;
    }
    else
    {
        g_protocol_last_rx_att_idx[conidx] = SP_IDX_CHAR1_VALUE;
    }
}

void Protocol_Auth_Clear(uint8_t conidx)
{
    if (conidx < PROTOCOL_MAX_CONN)
    {
        g_protocol_authed[conidx] = 0;
    }
}

void Protocol_Auth_Set(uint8_t conidx, bool ok)
{
    if (conidx < PROTOCOL_MAX_CONN)
    {
        g_protocol_authed[conidx] = ok ? 1 : 0;
    }
}

bool Protocol_Auth_IsOk(uint8_t conidx)
{
    if (conidx < PROTOCOL_MAX_CONN)
    {
        return g_protocol_authed[conidx] != 0;
    }
    return false;
}

void Protocol_Auth_SendResult(uint8_t conidx, bool ok)
{
    uint8_t  frame[PHONE_REPLY_FRAME_OVERHEAD + 32u];
    uint16_t frame_len = 0;

    /*
     * 协议要求：回复流水号必须与请求流水号一致。
     * - 优先使用最近一次收到的 seq。
     * - 若当前没有有效的 RX seq（例如无请求触发的异步通知），回退为本地自增。
     */
    uint8_t tx_seq = 0;
    if (conidx < PROTOCOL_MAX_CONN && g_protocol_last_rx_seq[conidx] != 0xFF)
    {
        tx_seq = g_protocol_last_rx_seq[conidx];
    }
    else
    {
        g_seq  = (g_seq + 1) & 0xFF;
        tx_seq = g_seq;
    }

    /*
     * 0x0101 Data 先按明文拼好，再根据 last_rx_crypto 做加密。
     * 为什么：App 发来的 Connect(0x01FE) 多数是 crypto=0x02(AES+PKCS7)，回包也需要同样加密方式才能被 App 正确解密。
     */
    uint8_t peer_mac[PHONE_REPLY_MODEL_MAC_MAX_LEN];
    uint8_t mac_len = 0;
    if (RSSI_Check_Get_Peer_Addr(conidx, peer_mac))
    {
        mac_len = PHONE_REPLY_MODEL_MAC_MAX_LEN;
    }

    uint8_t plain_payload[5u + PHONE_REPLY_MODEL_MAC_MAX_LEN];
    plain_payload[0] = ok ? 0 : 1; /* ResultCode: 0=成功，1=失败 */
    plain_payload[1] = 2u;         /* InductionStatus: 联调默认开 */
    plain_payload[2] = 2u;         /* NfcSwitch: 联调默认开 */
    plain_payload[3] = 0x02u;      /* CarSearchVolume: 联调默认中 */
    plain_payload[4] = mac_len;
    if (mac_len == PHONE_REPLY_MODEL_MAC_MAX_LEN)
    {
        memcpy(&plain_payload[5], peer_mac, PHONE_REPLY_MODEL_MAC_MAX_LEN);
    }
    uint16_t plain_len = (uint16_t)(5u + (uint16_t)mac_len);

    uint8_t crypto = CRYPTO_TYPE_NONE;
    if (conidx < PROTOCOL_MAX_CONN)
    {
        crypto = g_protocol_last_rx_crypto[conidx];
    }

    uint8_t  enc_payload[32u];
    uint16_t enc_len = 0;
    if (!proto_encrypt_payload(crypto,
                               plain_payload,
                               plain_len,
                               enc_payload,
                               (uint16_t)sizeof(enc_payload),
                               &enc_len))
    {
        co_printf("Protocol: auth result encrypt fail\r\n");
        return;
    }

#if PROTOCOL_DEBUG_TX
    co_printf("Protocol: AuthResult enc payload (%dB crypto=0x%02X): ",
              (int)enc_len,
              (unsigned)crypto);
    for (uint16_t i = 0; i < enc_len; i++)
    {
        co_printf("%02X ", enc_payload[i]);
    }
    co_printf("\r\n");
#endif

    if (!PhoneReply_BuildFrameEx(auth_result_ID,
                                 crypto,
                                 tx_seq,
                                 enc_payload,
                                 enc_len,
                                 frame,
                                 (uint16_t)sizeof(frame),
                                 &frame_len))
    {
        co_printf("Protocol: build auth result fail\r\n");
        return;
    }

#if PROTOCOL_DEBUG_TX
    /* 0x0101 回包属于 Notify：这里先打印，确认确实走到了发送逻辑 */
    co_printf("Protocol: AuthResult(0x0101) build ok=%d conidx=%d seq=%d "
              "frame_len=%d\r\n",
              ok ? 1 : 0,
              (int)conidx,
              (int)tx_seq,
              (int)frame_len);
#endif

    proto_send_frame(conidx, frame, frame_len);
}

void Protocol_Disconnect(uint8_t conidx)
{
    gap_disconnect_req(conidx);
}

void Protocol_Create_Test_Data(void)
{
    static uint8_t test_buf[20];
    uint8_t        i   = 0;
    uint8_t        bcc = 0;
    uint8_t        j   = 0;

    // 1. Header (0x5555)
    test_buf[i++] = 0x55;
    test_buf[i++] = 0x55;

    // 2. Length (Header 2 + Len 1 + Crypto 1 + Seq 1 + Cmd 2 + Data 1 + BCC 1 + Footer 2 = 11)
    test_buf[i++] = 11;

    // 3. Crypto
    test_buf[i++] = 0x00;

    // 4. Seq
    test_buf[i++] = 0x01;

    // 5. Cmd (0x0001) -> Little Endian 01 00
    test_buf[i++] = 0x01;
    test_buf[i++] = 0x00;

    // 6. Data (1 byte)
    test_buf[i++] = 0xEE;

    // 7. Calculate BCC (XOR from Header to Data)
    for (j = 0; j < i; j++)
    {
        bcc ^= test_buf[j];
    }
    test_buf[i++] = bcc;

    // 8. Footer (0xAAAA)
    test_buf[i++] = 0xAA;
    test_buf[i++] = 0xAA;

    co_printf("Protocol: Generated Test Data (%d bytes)\r\n", i);
    Protocol_Handle_Data(0, test_buf, i);
}

// 生成并喂入 FE Connect(0x01FE) 测试帧：Time(6)+Token(32)+mobileSystem(1)
void Protocol_Create_Test_FE_Connect_01FE(void)
{
    // total = Header(2)+Len(1)+Crypto(1)+Seq(1)+Cmd(2)+Payload(39)+BCC(1)+Footer(2) = 49
    static uint8_t test_buf[64];
    uint8_t        i   = 0;
    uint8_t        bcc = 0;

    // 1. Header (0x5555)
    test_buf[i++] = 0x55;
    test_buf[i++] = 0x55;

    // 2. Length
    test_buf[i++] = 49;

    // 3. Crypto
    test_buf[i++] = 0x00;

    // 4. Seq
    test_buf[i++] = 0x01;

    // 5. Cmd (0x01FE) Little Endian: FE 01
    test_buf[i++] = 0xFE;
    test_buf[i++] = 0x01;

    // 6. Payload
    // Time: 700101010101 -> BCD bytes: 70 01 01 01 01 01
    test_buf[i++] = 0x70;
    test_buf[i++] = 0x01;
    test_buf[i++] = 0x01;
    test_buf[i++] = 0x01;
    test_buf[i++] = 0x01;
    test_buf[i++] = 0x01;

    // Token(32): MD5(T-BOXKEY) 的 32 字节 ASCII 示例（用真实 token 替换这串即可）
    const char token32[] = "0123456789ABCDEF0123456789ABCDEF";
    for (uint8_t k = 0; k < 32; k++)
    {
        test_buf[i++] = (uint8_t)token32[k];
    }

    // mobileSystem: 1=android
    test_buf[i++] = 0x01;

    // 7. BCC (XOR from Header to end of Payload)
    for (uint8_t k = 0; k < i; k++)
    {
        bcc ^= test_buf[k];
    }
    test_buf[i++] = bcc;

    // 8. Footer (0xAAAA)
    test_buf[i++] = 0xAA;
    test_buf[i++] = 0xAA;

    co_printf("Protocol: Generated FE Connect 0x01FE (%d bytes)\r\n", (int)i);
    Protocol_Handle_Data(0, test_buf, i);
}

/* 发送帧到指定连接（通过 GATT Notify） */
static uint8_t proto_pick_tx_att_idx(uint8_t conidx)
{
    if (conidx >= PROTOCOL_MAX_CONN)
        return SP_IDX_CHAR1_VALUE;

#if PROTOCOL_TX_FORCE_FFF1
    /* 统一回包到 FFF1（SP_IDX_CHAR2_VALUE） */
    return SP_IDX_CHAR2_VALUE;
#else
    uint8_t prefer = g_protocol_last_rx_att_idx[conidx];
    if (prefer != SP_IDX_CHAR1_VALUE && prefer != SP_IDX_CHAR2_VALUE)
    {
        prefer = SP_IDX_CHAR1_VALUE;
    }

    if (prefer == SP_IDX_CHAR1_VALUE)
    {
        if (sp_is_char1_ntf_enabled(conidx))
            return SP_IDX_CHAR1_VALUE;
        if (sp_is_char2_ntf_enabled(conidx))
            return SP_IDX_CHAR2_VALUE;
        return SP_IDX_CHAR1_VALUE;
    }

    /* prefer == CHAR2 */
    if (sp_is_char2_ntf_enabled(conidx))
        return SP_IDX_CHAR2_VALUE;
    if (sp_is_char1_ntf_enabled(conidx))
        return SP_IDX_CHAR1_VALUE;
    return SP_IDX_CHAR2_VALUE;
#endif
}

static bool proto_send_frame(uint8_t conidx, const uint8_t* frame, uint16_t len)
{
    if (conidx >= PROTOCOL_MAX_CONN)
        return false;

    uint8_t att_idx = proto_pick_tx_att_idx(conidx);

#if PROTOCOL_DEBUG_TX
    /* cmd 在 frame[5..6]（小端），打印用于确认到底发了什么 */
    uint16_t cmd_be = 0;
    if (len >= 7)
    {
        /* Cmd：协议帧里是大端 */
        cmd_be = ((uint16_t)frame[5] << 8) | (uint16_t)frame[6];
    }
    co_printf("Protocol: TX pick att_idx=%d prefer=%d c1=%d c2=%d len=%d "
              "cmd=0x%04X\r\n",
              (int)att_idx,
              (int)g_protocol_last_rx_att_idx[conidx],
              sp_is_char1_ntf_enabled(conidx) ? 1 : 0,
              sp_is_char2_ntf_enabled(conidx) ? 1 : 0,
              (int)len,
              (unsigned)cmd_be);
    co_printf("Protocol: TX frame(all %dB): ", (int)len);
    for (uint16_t i = 0; i < len; i++)
    {
        co_printf("%02X ", frame[i]);
    }
    co_printf("\r\n");
#endif

    if (att_idx == SP_IDX_CHAR1_VALUE)
    {
        if (!sp_is_char1_ntf_enabled(conidx))
        {
            co_printf("Protocol: TX drop (char1 notify disabled) conidx=%d\r\n",
                      conidx);
            return false;
        }
    }
    else if (att_idx == SP_IDX_CHAR2_VALUE)
    {
        if (!sp_is_char2_ntf_enabled(conidx))
        {
            co_printf("Protocol: TX drop (char2 notify disabled) conidx=%d\r\n",
                      conidx);
            return false;
        }
    }

    ntf_data(conidx, att_idx, (uint8_t*)frame, len);

#if PROTOCOL_DEBUG_TX
    co_printf("Protocol: TX notify queued att_idx=%d len=%d\r\n",
              (int)att_idx,
              (int)len);
#endif
    return true;
}

int Protocol_Send_Unicast(uint8_t        conidx,
                          uint16_t       cmd,
                          const uint8_t* payload,
                          uint16_t       len)
{
    if (conidx >= PROTOCOL_MAX_CONN)
        return -1;
    if (gap_get_connect_status(conidx) == 0)
        return -2;
    if (len + 10 > PROTOCOL_MAX_LEN + 10)
        return -3;

    uint8_t* frame       = s_tx_frame_buf[conidx];
    uint8_t* enc_payload = s_tx_enc_buf[conidx];
    uint16_t enc_len     = 0;
    uint8_t  bcc         = 0;
    uint16_t total_len   = 0;

    /* 组帧 */
    frame[0] = 0x55;
    frame[1] = 0x55;

    /* 回包加密：默认跟随该连接最近一次请求的 crypto */
    uint8_t crypto = g_protocol_last_rx_crypto[conidx];
    if (!proto_encrypt_payload(crypto,
                               payload,
                               len,
                               enc_payload,
                               (uint16_t)PROTOCOL_MAX_LEN,
                               &enc_len))
    {
        return -5;
    }

#if PROTOCOL_DEBUG_TX
    co_printf("Protocol: Unicast enc payload (%dB crypto=0x%02X): ",
              (int)enc_len,
              (unsigned)crypto);
    for (uint16_t i = 0; i < enc_len; i++)
    {
        co_printf("%02X ", enc_payload[i]);
    }
    co_printf("\r\n");
#endif

    total_len = (uint16_t)(enc_len + 10u);
    frame[2]  = (uint8_t)total_len;
    frame[3]  = crypto;

    /* 同步应答：流水号与请求保持一致（若有） */
    uint8_t tx_seq = 0;
    if (conidx < PROTOCOL_MAX_CONN && g_protocol_last_rx_seq[conidx] != 0xFF)
    {
        tx_seq = g_protocol_last_rx_seq[conidx];
    }
    else
    {
        g_seq  = (g_seq + 1) & 0xFF;
        tx_seq = g_seq;
    }
    frame[4] = tx_seq;
    /* Cmd：协议帧里是大端 */
    frame[5] = (uint8_t)(cmd >> 8);
    frame[6] = (uint8_t)(cmd & 0xFF);

    if (enc_len > 0)
    {
        memcpy(&frame[7], enc_payload, enc_len);
    }

    for (uint16_t i = 0; i < (uint16_t)(7u + enc_len); i++)
    {
        bcc ^= frame[i];
    }
    frame[7 + enc_len] = bcc;
    frame[8 + enc_len] = 0xAA;
    frame[9 + enc_len] = 0xAA;

    /* 发送：内部会按 last_rx_att_idx 选通道，并检查 notify 是否开启 */
    return proto_send_frame(conidx, frame, total_len) ? 0 : -4;
}

int Protocol_Send_Unicast_Async(uint8_t        conidx,
                                uint16_t       cmd,
                                const uint8_t* payload,
                                uint16_t       len)
{
    if (conidx >= PROTOCOL_MAX_CONN)
        return -1;
    if (gap_get_connect_status(conidx) == 0)
        return -2;
    if (len + 10 > PROTOCOL_MAX_LEN + 10)
        return -3;

    uint8_t* frame       = s_tx_frame_buf[conidx];
    uint8_t* enc_payload = s_tx_enc_buf[conidx];
    uint16_t enc_len     = 0;
    uint8_t  bcc         = 0;
    uint16_t total_len   = 0;

    /* 组帧 */
    frame[0] = 0x55;
    frame[1] = 0x55;

    /* 主动推送：加密策略跟随该连接最近一次请求的 crypto */
    uint8_t crypto = g_protocol_last_rx_crypto[conidx];
    if (!proto_encrypt_payload(crypto,
                               payload,
                               len,
                               enc_payload,
                               (uint16_t)PROTOCOL_MAX_LEN,
                               &enc_len))
    {
        return -5;
    }

    total_len = (uint16_t)(enc_len + 10u);
    frame[2]  = (uint8_t)total_len;
    frame[3]  = crypto;

    /* 强制使用新的流水号（避免复用 last_rx_seq 被 APP 当成上一次指令应答） */
    g_seq    = (g_seq + 1) & 0xFF;
    frame[4] = g_seq;

    /* Cmd：协议帧里是大端 */
    frame[5] = (uint8_t)(cmd >> 8);
    frame[6] = (uint8_t)(cmd & 0xFF);

    if (enc_len > 0)
    {
        memcpy(&frame[7], enc_payload, enc_len);
    }

    for (uint16_t i = 0; i < (uint16_t)(7u + enc_len); i++)
    {
        bcc ^= frame[i];
    }
    frame[7 + enc_len] = bcc;
    frame[8 + enc_len] = 0xAA;
    frame[9 + enc_len] = 0xAA;

    return proto_send_frame(conidx, frame, total_len) ? 0 : -4;
}
/* 发送 ACK（Cmd=0x0000，Data 长度=0） */
#if PROTOCOL_USE_ACK
static void proto_send_ack(uint8_t conidx, uint8_t seq)
{
    uint8_t ack[10];
    uint8_t bcc = 0;
    ack[0]      = 0x55;
    ack[1]      = 0x55;
    ack[2]      = 10; // 总长度
    ack[3]      = CRYPTO_TYPE_NONE;
    ack[4]      = seq;
    ack[5]      = 0x00;
    ack[6]      = 0x00; // CMD_ACK_ID（大端/小端一致）
    for (uint8_t i = 0; i < 7; i++)
        bcc ^= ack[i];
    ack[7] = bcc;
    ack[8] = 0xAA;
    ack[9] = 0xAA;
    proto_send_frame(conidx, ack, sizeof(ack));
}
#endif

/* 重传定时器回调 */
#if PROTOCOL_USE_ACK
static void proto_ack_timeout(void* arg)
{
    uint8_t conidx = (uint8_t)(uint32_t)arg;
    if (conidx >= PROTOCOL_MAX_CONN)
        return;
    proto_tx_ctx_t* ctx = &g_tx_ctx[conidx];
    if (!ctx->in_flight)
        return;

    if (ctx->retry < PROTOCOL_MAX_RETRY)
    {
        ctx->retry++;
        co_printf("Protocol: RETRY conidx=%d seq=%d try=%d\r\n",
                  conidx,
                  ctx->seq,
                  ctx->retry);
        proto_send_frame(conidx, ctx->buf, ctx->len);
        proto_restart_timer(conidx);
    }
    else
    {
        co_printf(
            "Protocol: RETRY FAIL conidx=%d seq=%d\r\n", conidx, ctx->seq);
        ctx->in_flight = false;
    }
}

static void proto_restart_timer(uint8_t conidx)
{
    os_timer_stop(&g_ack_timer[conidx]);
    os_timer_start(&g_ack_timer[conidx], PROTOCOL_ACK_TIMEOUT, 0);
}
#endif

/* 业务层调用：广播（逐连接）发送，开启 ACK/重传 */
/**
 * @brief 业务层发送带自动 ACK/重传的广播（逐连接通知）
 * - 自动为每个已连接且已订阅 CHAR1 通知的 conidx 发送一帧
 * - 自动填写 Length 字段、BCC、Header/Footer、流水号（全局递增）
 * - 启动 3 秒 ACK 定时，最多重传 3 次；critical=true 时重传失败会断开该连接
 * @param cmd       命令标识
 * @param payload   负载指针（可为 NULL，表示无数据段）
 * @param len       负载长度（不含协议头尾）；若用编译期数组，可用 Protocol_Send_Broadcast_ARRAY 自动填 len
 * @param critical  是否关键数据（重传 3 次仍失败则断链）
 * @return 0 成功发起；-2 长度超限；-3 无连接/未订阅；其他负数表示失败
 */
int Protocol_Send_Broadcast(uint16_t       cmd,
                            const uint8_t* payload,
                            uint16_t       len,
                            bool           critical)
{
    if (len + 10 > PROTOCOL_MAX_LEN + 10)
        return -2; // 长度超限

    /*
	 * 广播：不同连接的 last_rx_crypto 可能不同，因此需要“逐连接组帧并加密”。
	 * 为什么：否则 crypto=0x02 的连接会拿到明文/crypto=0 的帧，App 端会解密失败。
	 */
    g_seq          = (g_seq + 1) & 0xFF;
    uint8_t tx_seq = g_seq;

#if PROTOCOL_USE_ACK
    /* 逐连接发送，仅对已连接且已打开 CHAR1 通知的连接生效 */
    bool sent_any = false;
    for (uint8_t idx = 0; idx < PROTOCOL_MAX_CONN; idx++)
    {
        if (gap_get_connect_status(idx) == 0)
            continue;

        uint8_t* frame       = s_tx_frame_buf[idx];
        uint8_t* enc_payload = s_tx_enc_buf[idx];
        uint16_t enc_len     = 0;
        uint8_t  bcc         = 0;
        uint8_t  crypto      = g_protocol_last_rx_crypto[idx];
        if (!proto_encrypt_payload(crypto,
                                   payload,
                                   len,
                                   enc_payload,
                                   (uint16_t)PROTOCOL_MAX_LEN,
                                   &enc_len))
        {
            continue;
        }
        uint16_t total_len = (uint16_t)(enc_len + 10u);

        frame[0] = 0x55;
        frame[1] = 0x55;
        frame[2] = (uint8_t)total_len;
        frame[3] = crypto;
        frame[4] = tx_seq;
        /* Cmd：协议帧里是大端 */
        frame[5] = (uint8_t)(cmd >> 8);
        frame[6] = (uint8_t)(cmd & 0xFF);
        if (enc_len > 0)
            memcpy(&frame[7], enc_payload, enc_len);
        for (uint16_t i = 0; i < (uint16_t)(7u + enc_len); i++)
            bcc ^= frame[i];
        frame[7 + enc_len] = bcc;
        frame[8 + enc_len] = 0xAA;
        frame[9 + enc_len] = 0xAA;

        /* 由 proto_send_frame 内部按最后 RX 通道选择 CHAR1/CHAR2，并检查 notify 使能 */
        proto_tx_ctx_t* ctx = &g_tx_ctx[idx];
        memcpy(ctx->buf, frame, total_len);
        ctx->len       = total_len;
        ctx->seq       = tx_seq;
        ctx->cmd       = cmd;
        ctx->retry     = 0;
        ctx->critical  = critical;
        ctx->in_flight = true;

        if (proto_send_frame(idx, frame, total_len))
        {
            proto_restart_timer(idx);
            sent_any = true;
        }
        else
        {
            ctx->in_flight = false;
        }
    }

    if (!sent_any)
    {
        return -3; // 没有任何连接/订阅者
    }
    return 0;
#else
    /* 开发阶段：不启用 ACK/重传，逐连接发送（按最后 RX 通道选择 CHAR1/CHAR2） */
    bool sent_any = false;
    for (uint8_t idx = 0; idx < PROTOCOL_MAX_CONN; idx++)
    {
        if (gap_get_connect_status(idx) == 0)
            continue;

        uint8_t* frame       = s_tx_frame_buf[idx];
        uint8_t* enc_payload = s_tx_enc_buf[idx];
        uint16_t enc_len     = 0;
        uint8_t  bcc         = 0;
        uint8_t  crypto      = g_protocol_last_rx_crypto[idx];
        if (!proto_encrypt_payload(crypto,
                                   payload,
                                   len,
                                   enc_payload,
                                   (uint16_t)PROTOCOL_MAX_LEN,
                                   &enc_len))
        {
            continue;
        }
        uint16_t total_len = (uint16_t)(enc_len + 10u);

        frame[0] = 0x55;
        frame[1] = 0x55;
        frame[2] = (uint8_t)total_len;
        frame[3] = crypto;
        frame[4] = tx_seq;
        /* Cmd：协议帧里是大端 */
        frame[5] = (uint8_t)(cmd >> 8);
        frame[6] = (uint8_t)(cmd & 0xFF);
        if (enc_len > 0)
            memcpy(&frame[7], enc_payload, enc_len);
        for (uint16_t i = 0; i < (uint16_t)(7u + enc_len); i++)
            bcc ^= frame[i];
        frame[7 + enc_len] = bcc;
        frame[8 + enc_len] = 0xAA;
        frame[9 + enc_len] = 0xAA;

        if (proto_send_frame(idx, frame, total_len))
        {
            sent_any = true;
        }
    }
    return sent_any ? 0 : -3;
#endif
}
