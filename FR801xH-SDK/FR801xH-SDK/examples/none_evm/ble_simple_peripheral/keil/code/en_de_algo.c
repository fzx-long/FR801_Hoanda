/*********************************************************************
 * @file en_de_algo.c
 * @author Fanzx (1456925916@qq.com)
 * @brief 加解密算法实现 (OOP 动态绑定)
 * @version 0.1
 * @date 2026-01-06
 *********************************************************************/

#include "en_de_algo.h"
#include <string.h>
/*
 * 使用 SDK 已提供的 AES 实现（components/modules/aes_cbc）。
 * 为什么：当前工程内手写 AES 解密在联调中出现“解密后明文随机”的现象，
 *         会直接导致 0x01FE 的 Time/Token/mobileSystem 解析失败，进而鉴权失败并断连。
 * 这里优先改为使用 SDK 内置 AES，减少算法实现偏差。
 */
#include "../keil/components/modules/aes_cbc/aes_cbc.h"

/* ==================== 具体策略实现 (Strategy Implementations) ==================== */

/* --- 策略 1: None (直通) --- */
static bool ops_none_encrypt(Algo_Context_t* ctx,
                             const uint8_t*  in,
                             uint32_t        len,
                             uint8_t*        out) {
    (void)ctx;
    // 不加密，直接拷贝
    memmove(out, in, len);
    return true;
}

static bool ops_none_decrypt(Algo_Context_t* ctx,
                             const uint8_t*  in,
                             uint32_t        len,
                             uint8_t*        out) {
    (void)ctx;
    memmove(out, in, len);
    return true;
}

static const Algo_Ops_t s_ops_none = {
    .name       = "None",
    .block_size = 1, // 任意对齐
    .encrypt    = ops_none_encrypt,
    .decrypt    = ops_none_decrypt,
};

/* --- 策略 2: AES-128-CBC --- */

/*
 * 重要说明：协议层 crypto=0x02 在文档中叫 AES128，但你截图工具给出的参数是
 * AES-128-ECB + PKCS7Padding。
 * 
 * 为了兼容：
 * - 默认走 ECB（ALGO_AES_USE_ECB=1）
 * - 如后续确认 App 端实际为 CBC，再把宏置 0 回退。
 */
#ifndef ALGO_AES_USE_ECB
#define ALGO_AES_USE_ECB 1
#endif

#define ALGO_AES_BLOCK_SIZE 16u

#if 0
/* 旧实现：手写 AES（已停用，避免误用/误差导致解密错误）。
 * 如果需要对比验证，可临时恢复，但联调建议以 SDK AES 为准。
 */

// S-box
static const uint8_t sbox[256] = {
  //0     1    2      3     4    5     6     7      8    9     A      B    C     D     E     F
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };

static const uint8_t rsbox[256] = {
  0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
  0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
  0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
  0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
  0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
  0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
  0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
  0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
  0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
  0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
  0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
  0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
  0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
  0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
  0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
  0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d };

static const uint8_t Rcon[11] = {
  0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };

#define Nb 4
#define Nk 4
#define Nr 10

typedef struct {
	uint32_t round_key[44];
} AesKey_t;

static void KeyExpansion(const uint8_t* key, AesKey_t* ctx) {
  uint32_t i, j, k;
  uint8_t tempa[4]; 
  
  for(i = 0; i < Nk; ++i) {
    ctx->round_key[i] = ((uint32_t)key[4 * i] << 24) | ((uint32_t)key[4 * i + 1] << 16) |
                        ((uint32_t)key[4 * i + 2] << 8) | (uint32_t)key[4 * i + 3];
  }

  for(i = Nk; i < Nb * (Nr + 1); ++i) {
    k = ctx->round_key[i - 1];
    tempa[0] = (k >> 24) & 0xff;
    tempa[1] = (k >> 16) & 0xff;
    tempa[2] = (k >> 8) & 0xff;
    tempa[3] = k & 0xff;
    
    if (i % Nk == 0) {
      const uint8_t u8tmp = tempa[0];
      tempa[0] = sbox[tempa[1]];
      tempa[1] = sbox[tempa[2]];
      tempa[2] = sbox[tempa[3]];
      tempa[3] = sbox[u8tmp];
      tempa[0] = tempa[0] ^ Rcon[i/Nk];
    }
    
    uint32_t temp_word = ((uint32_t)tempa[0] << 24) | ((uint32_t)tempa[1] << 16) |
                         ((uint32_t)tempa[2] << 8) | (uint32_t)tempa[3];
                         
    ctx->round_key[i] = ctx->round_key[i - Nk] ^ temp_word;
  }
}

#define getSBoxValue(num)  (sbox[(num)])
#define getSBoxInvert(num) (rsbox[(num)])

/* Helper functions for cipher */
static uint8_t multiply(uint8_t x, uint8_t y) {
  return (((y & 1) * x) ^
       ((y>>1 & 1) * ((x<<1) ^ ((x>>7) * 0x1b))) ^
       ((y>>2 & 1) * ((x<<2) ^ ((x>>6) * 0x1b) ^ ((x>>7) * 0x36))) ^
       ((y>>3 & 1) * ((x<<3) ^ ((x>>5) * 0x1b) ^ ((x>>6) * 0x36) ^ ((x>>7) * 0x6c))) ^
       ((y>>4 & 1) * ((x<<4) ^ ((x>>4) * 0x1b) ^ ((x>>5) * 0x36) ^ ((x>>6) * 0x6c) ^ ((x>>7) * 0xd8))));
}

static void EncryptBlock(const AesKey_t* ctx, uint8_t* state) {
    // Basic AES encrypt implementation omitted for brevity unless needed.
    // Focusing on Decrypt as that's the current problem.
    // If you need encryption, I can add it. 
    // Implementing DecryptBlock properly:
    (void)ctx; (void)state; // Placeholder
}

static void DecryptBlock(const AesKey_t* ctx, uint8_t* buf) {
    uint32_t i, j, round;
    uint8_t state[4][4];
    
    for(i=0; i<4; i++) for(j=0; j<4; j++) state[j][i] = buf[i*4 + j];
    
    /* AddRoundKey */
    for(i=0; i<4; i++) {
         uint32_t k = ctx->round_key[Nr * Nb + i];
         state[0][i] ^= (k >> 24) & 0xff;
         state[1][i] ^= (k >> 16) & 0xff;
         state[2][i] ^= (k >> 8) & 0xff;
         state[3][i] ^= k & 0xff;
    }

    for(round=Nr-1; round>0; round--) {
        /* InvShiftRows */
        uint8_t t;
        t = state[3][1]; state[3][1] = state[2][1]; state[2][1] = state[1][1]; state[1][1] = state[0][1]; state[0][1] = t;
        t = state[0][2]; state[0][2] = state[2][2]; state[2][2] = t;
        t = state[1][2]; state[1][2] = state[3][2]; state[3][2] = t;
        t = state[0][3]; state[0][3] = state[1][3]; state[1][3] = state[2][3]; state[2][3] = state[3][3]; state[3][3] = t;
        
        /* InvSubBytes */
        for(i=0; i<4; i++) for(j=0; j<4; j++) state[j][i] = getSBoxInvert(state[j][i]);

        /* AddRoundKey */
        for(i=0; i<4; i++) {
             uint32_t k = ctx->round_key[round * Nb + i];
             state[0][i] ^= (k >> 24) & 0xff;
             state[1][i] ^= (k >> 16) & 0xff;
             state[2][i] ^= (k >> 8) & 0xff;
             state[3][i] ^= k & 0xff;
        }

        /* InvMixColumns */
        for(i=0; i<4; i++) {
             uint8_t a = state[0][i], b = state[1][i], c = state[2][i], d = state[3][i];
             state[0][i] = multiply(a, 0x0e) ^ multiply(b, 0x0b) ^ multiply(c, 0x0d) ^ multiply(d, 0x09);
             state[1][i] = multiply(a, 0x09) ^ multiply(b, 0x0e) ^ multiply(c, 0x0b) ^ multiply(d, 0x0d);
             state[2][i] = multiply(a, 0x0d) ^ multiply(b, 0x09) ^ multiply(c, 0x0e) ^ multiply(d, 0x0b);
             state[3][i] = multiply(a, 0x0b) ^ multiply(b, 0x0d) ^ multiply(c, 0x09) ^ multiply(d, 0x0e);
        }
    }

    /* InvShiftRows */
    uint8_t t;
    t = state[3][1]; state[3][1] = state[2][1]; state[2][1] = state[1][1]; state[1][1] = state[0][1]; state[0][1] = t;
    t = state[0][2]; state[0][2] = state[2][2]; state[2][2] = t;
    t = state[1][2]; state[1][2] = state[3][2]; state[3][2] = t;
    t = state[0][3]; state[0][3] = state[1][3]; state[1][3] = state[2][3]; state[2][3] = state[3][3]; state[3][3] = t;

    /* InvSubBytes */
    for(i=0; i<4; i++) for(j=0; j<4; j++) state[j][i] = getSBoxInvert(state[j][i]);

    /* AddRoundKey */
    for(i=0; i<4; i++) {
         uint32_t k = ctx->round_key[0 * Nb + i];
         state[0][i] ^= (k >> 24) & 0xff;
         state[1][i] ^= (k >> 16) & 0xff;
         state[2][i] ^= (k >> 8) & 0xff;
         state[3][i] ^= k & 0xff;
    }

    for(i=0; i<4; i++) for(j=0; j<4; j++) buf[i*4 + j] = state[j][i];
}

#endif /* #if 0 legacy AES */

static bool ops_aes_encrypt(Algo_Context_t* ctx,
                            const uint8_t*  in,
                            uint32_t        len,
                            uint8_t*        out) {
    if (ctx == NULL || in == NULL || out == NULL) {
        return false;
    }
    if ((len % ALGO_AES_BLOCK_SIZE) != 0u) {
        return false;
    }

#if ALGO_AES_USE_ECB
    /*
     * ECB：通过“每个 16B 块都用 IV=0 单独做一次 CBC 加密”来实现。
     * 为什么这样做：SDK 提供的是 AES_cbc_encrypt，而 ECB 只需要 AES_ENC(C_i)。
     * 对单块而言，CBC(IV=0) 等价 ECB。
     */
    static const uint8_t zero_iv[ALGO_AES_BLOCK_SIZE] = {0};
    for (uint32_t offset = 0; offset < len; offset += ALGO_AES_BLOCK_SIZE) {
        uint8_t block_in[ALGO_AES_BLOCK_SIZE];
        uint8_t block_out[ALGO_AES_BLOCK_SIZE];
        AES_CTX aes;

        memcpy(block_in, &in[offset], ALGO_AES_BLOCK_SIZE);
        AES_set_key(&aes, ctx->key, zero_iv, AES_MODE_128);
        AES_cbc_encrypt(&aes, block_in, block_out, (int)ALGO_AES_BLOCK_SIZE);
        memcpy(&out[offset], block_out, ALGO_AES_BLOCK_SIZE);
    }
    return true;
#else
    /* CBC：直接调用 SDK CBC（IV 使用 ctx->iv） */
    AES_CTX aes;
    AES_set_key(&aes, ctx->key, ctx->iv, AES_MODE_128);
    AES_cbc_encrypt(&aes, in, out, (int)len);
    return true;
#endif
}

static bool ops_aes_decrypt(Algo_Context_t* ctx,
                            const uint8_t*  in,
                            uint32_t        len,
                            uint8_t*        out) {
    if (ctx == NULL || in == NULL || out == NULL) {
        return false;
    }
    if ((len % ALGO_AES_BLOCK_SIZE) != 0u) {
        return false;
    }

#if ALGO_AES_USE_ECB
    /*
     * ECB：通过“每个 16B 块都用 IV=0 单独做一次 CBC 解密”来实现。
     * 为什么：对单块而言，CBC 解密 P = AES_DEC(C) XOR IV；当 IV=0 时等价 ECB。
     * 注意：不能直接一次性调用 AES_cbc_decrypt 处理多块，因为 CBC 会链式更新 IV。
     */
    static const uint8_t zero_iv[ALGO_AES_BLOCK_SIZE] = {0};
    for (uint32_t offset = 0; offset < len; offset += ALGO_AES_BLOCK_SIZE) {
        uint8_t block_in[ALGO_AES_BLOCK_SIZE];
        uint8_t block_out[ALGO_AES_BLOCK_SIZE];
        AES_CTX aes;

        memcpy(block_in, &in[offset], ALGO_AES_BLOCK_SIZE);
        AES_set_key(&aes, ctx->key, zero_iv, AES_MODE_128);
        AES_convert_key(&aes);
        AES_cbc_decrypt(&aes, block_in, block_out, (int)ALGO_AES_BLOCK_SIZE);
        memcpy(&out[offset], block_out, ALGO_AES_BLOCK_SIZE);
    }
    return true;
#else
    /* CBC：直接调用 SDK CBC（IV 使用 ctx->iv） */
    AES_CTX aes;
    AES_set_key(&aes, ctx->key, ctx->iv, AES_MODE_128);
    AES_convert_key(&aes);
    AES_cbc_decrypt(&aes, in, out, (int)len);
    return true;
#endif
}

static const Algo_Ops_t s_ops_aes = {
    .name       = "AES-128",
    .block_size = 16,
    .encrypt    = ops_aes_encrypt,
    .decrypt    = ops_aes_decrypt,
};

/* ==================== 工厂/绑定实现 (Factory) ==================== */

bool Algo_Bind(Algo_Context_t* ctx, algo_type_t type) {
    if (ctx == NULL)
        return false;

    ctx->type = type;

    switch (type) {
    case ALGO_TYPE_NONE: ctx->ops = &s_ops_none; break;
    case ALGO_TYPE_AES_CBC: ctx->ops = &s_ops_aes; break;
    default:
        // 默认回退到 None
        ctx->ops = &s_ops_none;
        return false;
    }
    return true;
}

void Algo_SetKeyIV(Algo_Context_t* ctx, const uint8_t* key, const uint8_t* iv) {
    if (ctx == NULL) {
        return;
    }

    /*
     * 关键修复：不同算法的 Key 长度不同。
     * 之前无条件 memcpy(ALGO_MAX_KEY_LEN=32) 会在 key 实参只有 16 字节时发生越界读，
     * 导致解密结果不稳定（time_bcd 解析失败、鉴权必失败等）。
     */
    uint8_t key_len = 0;
    switch (ctx->type) {
    case ALGO_TYPE_AES_CBC:
        key_len = 16; /* AES-128 */
        break;
    case ALGO_TYPE_DES3:
        key_len = 24; /* 3DES(168) 常见为 24 字节 key */
        break;
    case ALGO_TYPE_NONE:
    default: key_len = 0; break;
    }

    if (key != NULL) {
        memset(ctx->key, 0, ALGO_MAX_KEY_LEN);
        if (key_len > 0) {
            memcpy(ctx->key, key, key_len);
        }
    }

    if (iv != NULL) {
        memset(ctx->iv, 0, ALGO_MAX_IV_LEN);
        memcpy(ctx->iv, iv, ALGO_MAX_IV_LEN);
    }
}

uint32_t Algo_Padding(uint8_t* buf, uint32_t data_len, uint8_t block_size) {
    if (block_size == 0)
        return data_len;
    uint8_t pad = block_size - (data_len % block_size);
    for (uint8_t i = 0; i < pad; i++) {
        buf[data_len + i] = pad;
    }
    return data_len + pad;
}

/* ==================== MD5 Implementation ==================== */

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t  buffer[64];
} MD5_CTX;

#define MD5_GET_UINT32_LE(n, b, i)                                             \
    {                                                                          \
        (n) = ((uint32_t)(b)[(i)]) | ((uint32_t)(b)[(i) + 1] << 8) |           \
              ((uint32_t)(b)[(i) + 2] << 16) | ((uint32_t)(b)[(i) + 3] << 24); \
    }

#define MD5_PUT_UINT32_LE(n, b, i)                                             \
    {                                                                          \
        (b)[(i)]     = (uint8_t)((n));                                         \
        (b)[(i) + 1] = (uint8_t)((n) >> 8);                                    \
        (b)[(i) + 2] = (uint8_t)((n) >> 16);                                   \
        (b)[(i) + 3] = (uint8_t)((n) >> 24);                                   \
    }

static void MD5_Init(MD5_CTX* ctx) {
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0]                 = 0x67452301;
    ctx->state[1]                 = 0xEFCDAB89;
    ctx->state[2]                 = 0x98BADCFE;
    ctx->state[3]                 = 0x10325476;
}

static void MD5_Process(MD5_CTX* ctx, const uint8_t data[64]) {
    uint32_t X[16], A, B, C, D;
    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];

    MD5_GET_UINT32_LE(X[0], data, 0);
    MD5_GET_UINT32_LE(X[1], data, 4);
    MD5_GET_UINT32_LE(X[2], data, 8);
    MD5_GET_UINT32_LE(X[3], data, 12);
    MD5_GET_UINT32_LE(X[4], data, 16);
    MD5_GET_UINT32_LE(X[5], data, 20);
    MD5_GET_UINT32_LE(X[6], data, 24);
    MD5_GET_UINT32_LE(X[7], data, 28);
    MD5_GET_UINT32_LE(X[8], data, 32);
    MD5_GET_UINT32_LE(X[9], data, 36);
    MD5_GET_UINT32_LE(X[10], data, 40);
    MD5_GET_UINT32_LE(X[11], data, 44);
    MD5_GET_UINT32_LE(X[12], data, 48);
    MD5_GET_UINT32_LE(X[13], data, 52);
    MD5_GET_UINT32_LE(X[14], data, 56);
    MD5_GET_UINT32_LE(X[15], data, 60);

#define S(x, n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))
#define P(a, b, c, d, k, s, t)                                                 \
    {                                                                          \
        a += F(b, c, d) + X[k] + t;                                            \
        a = S(a, s) + b;                                                       \
    }

#define F(x, y, z) (z ^ (x & (y ^ z)))
    P(A, B, C, D, 0, 7, 0xD76AA478);
    P(D, A, B, C, 1, 12, 0xE8C7B756);
    P(C, D, A, B, 2, 17, 0x242070DB);
    P(B, C, D, A, 3, 22, 0xC1BDCEEE);
    P(A, B, C, D, 4, 7, 0xF57C0FAF);
    P(D, A, B, C, 5, 12, 0x4787C62A);
    P(C, D, A, B, 6, 17, 0xA8304613);
    P(B, C, D, A, 7, 22, 0xFD469501);
    P(A, B, C, D, 8, 7, 0x698098D8);
    P(D, A, B, C, 9, 12, 0x8B44F7AF);
    P(C, D, A, B, 10, 17, 0xFFFF5BB1);
    P(B, C, D, A, 11, 22, 0x895CD7BE);
    P(A, B, C, D, 12, 7, 0x6B901122);
    P(D, A, B, C, 13, 12, 0xFD987193);
    P(C, D, A, B, 14, 17, 0xA679438E);
    P(B, C, D, A, 15, 22, 0x49B40821);

#undef F
#define F(x, y, z) (y ^ (z & (x ^ y)))
    P(A, B, C, D, 1, 5, 0xF61E2562);
    P(D, A, B, C, 6, 9, 0xC040B340);
    P(C, D, A, B, 11, 14, 0x265E5A51);
    P(B, C, D, A, 0, 20, 0xE9B6C7AA);
    P(A, B, C, D, 5, 5, 0xD62F105D);
    P(D, A, B, C, 10, 9, 0x02441453);
    P(C, D, A, B, 15, 14, 0xD8A1E681);
    P(B, C, D, A, 4, 20, 0xE7D3FBC8);
    P(A, B, C, D, 9, 5, 0x21E1CDE6);
    P(D, A, B, C, 14, 9, 0xC33707D6);
    P(C, D, A, B, 3, 14, 0xF4D50D87);
    P(B, C, D, A, 8, 20, 0x455A14ED);
    P(A, B, C, D, 13, 5, 0xA9E3E905);
    P(D, A, B, C, 2, 9, 0xFCEFA3F8);
    P(C, D, A, B, 7, 14, 0x676F02D9);
    P(B, C, D, A, 12, 20, 0x8D2A4C8A);

#undef F
#define F(x, y, z) (x ^ y ^ z)
    P(A, B, C, D, 5, 4, 0xFFFA3942);
    P(D, A, B, C, 8, 11, 0x8771F681);
    P(C, D, A, B, 11, 16, 0x6D9D6122);
    P(B, C, D, A, 14, 23, 0xFDE5380C);
    P(A, B, C, D, 1, 4, 0xA4BEEA44);
    P(D, A, B, C, 4, 11, 0x4BDECFA9);
    P(C, D, A, B, 7, 16, 0xF6BB4B60);
    P(B, C, D, A, 10, 23, 0xBEBFBC70);
    P(A, B, C, D, 13, 4, 0x289B7EC6);
    P(D, A, B, C, 0, 11, 0xEAA127FA);
    P(C, D, A, B, 3, 16, 0xD4EF3085);
    P(B, C, D, A, 6, 23, 0x04881D05);
    P(A, B, C, D, 9, 4, 0xD9D4D039);
    P(D, A, B, C, 12, 11, 0xE6DB99E5);
    P(C, D, A, B, 15, 16, 0x1FA27CF8);
    P(B, C, D, A, 2, 23, 0xC4AC5665);

#undef F
#define F(x, y, z) (y ^ (x | ~z))
    P(A, B, C, D, 0, 6, 0xF4292244);
    P(D, A, B, C, 7, 10, 0x432AFF97);
    P(C, D, A, B, 14, 15, 0xAB9423A7);
    P(B, C, D, A, 5, 21, 0xFC93A039);
    P(A, B, C, D, 12, 6, 0x655B59C3);
    P(D, A, B, C, 3, 10, 0x8F0CCC92);
    P(C, D, A, B, 10, 15, 0xFFEFF47D);
    P(B, C, D, A, 1, 21, 0x85845DD1);
    P(A, B, C, D, 8, 6, 0x6FA87E4F);
    P(D, A, B, C, 15, 10, 0xFE2CE6E0);
    P(C, D, A, B, 6, 15, 0xA3014314);
    P(B, C, D, A, 13, 21, 0x4E0811A1);
    P(A, B, C, D, 4, 6, 0xF7537E82);
    P(D, A, B, C, 11, 10, 0xBD3AF235);
    P(C, D, A, B, 2, 15, 0x2AD7D2BB);
    P(B, C, D, A, 9, 21, 0xEB86D391);

#undef F

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
}

static void MD5_Update(MD5_CTX* ctx, const uint8_t* input, uint32_t ilen) {
    uint32_t fill;
    uint32_t left;

    if (ilen == 0)
        return;

    left = ctx->count[0] & 0x3F;
    fill = 64 - left;

    ctx->count[0] += ilen;
    ctx->count[0] &= 0xFFFFFFFF;
    if (ctx->count[0] < ilen)
        ctx->count[1]++;

    if (left && ilen >= fill) {
        memcpy(ctx->buffer + left, input, fill);
        MD5_Process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }

    while (ilen >= 64) {
        MD5_Process(ctx, input);
        input += 64;
        ilen -= 64;
    }

    if (ilen > 0) {
        memcpy(ctx->buffer + left, input, ilen);
    }
}

static void MD5_Final(MD5_CTX* ctx, uint8_t output[16]) {
    uint32_t used = ctx->count[0] & 0x3F;
    uint8_t  mid_buf[64];

    mid_buf[0] = 0x80;
    if (used + 1 > 56) {
        memset(ctx->buffer + used + 1, 0, 64 - used - 1);
        MD5_Process(ctx, ctx->buffer);
        memset(ctx->buffer, 0, 56);
    } else {
        memset(ctx->buffer + used + 1, 0, 56 - used - 1);
    }

    MD5_PUT_UINT32_LE(ctx->count[0] << 3, ctx->buffer, 56);
    MD5_PUT_UINT32_LE(
        ctx->count[1] << 3 | ctx->count[0] >> 29, ctx->buffer, 60);

    MD5_Process(ctx, ctx->buffer);
    MD5_PUT_UINT32_LE(ctx->state[0], output, 0);
    MD5_PUT_UINT32_LE(ctx->state[1], output, 4);
    MD5_PUT_UINT32_LE(ctx->state[2], output, 8);
    MD5_PUT_UINT32_LE(ctx->state[3], output, 12);
}

void Algo_MD5_Calc(const uint8_t* in, uint32_t len, uint8_t out[16]) {
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, in, len);
    MD5_Final(&ctx, out);
}
