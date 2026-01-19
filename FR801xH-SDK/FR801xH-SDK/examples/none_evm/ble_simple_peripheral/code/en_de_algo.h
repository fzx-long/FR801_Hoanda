/*********************************************************************
 * @file en_de_algo.h
 * @author Fanzx (1456925916@qq.com)
 * @brief 加解密算法抽象层 (Strategy Pattern)
 * @version 0.1
 * @date 2026-01-06
 *********************************************************************/

#ifndef EN_DE_ALGO_H
#define EN_DE_ALGO_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declaration */
struct Algo_Context_s;

/**
 * @brief 算法操作集接口 (V-Table)
 */
typedef struct {
    const char *name;           /**< 算法名称 */
    uint8_t block_size;         /**< 块大小 (字节) */
    
    /**
     * @brief 加密函数接口
     */
    bool (*encrypt)(struct Algo_Context_s *ctx, const uint8_t *in, uint32_t len, uint8_t *out);
    
    /**
     * @brief 解密函数接口
     */
    bool (*decrypt)(struct Algo_Context_s *ctx, const uint8_t *in, uint32_t len, uint8_t *out);
} Algo_Ops_t;

/**
 * @brief 支持的算法类型枚举
 */
typedef enum {
    ALGO_TYPE_NONE = 0x00,      /**< 直通模式 (无加密) */
    ALGO_TYPE_DES3 = 0x01,      /**< DES3 模式 */
    ALGO_TYPE_AES_CBC = 0x02,   /**< AES-128 CBC 模式 */
} algo_type_t;

#define ALGO_MAX_KEY_LEN 32     /**< 最大密钥长度 (256 bits) */
#define ALGO_MAX_IV_LEN  16     /**< 最大向量长度 (128 bits) */

/**
 * @brief 算法上下文 (动态对象)
 */
typedef struct Algo_Context_s {
    algo_type_t type;           /**< 当前绑定的算法类型 */
    const Algo_Ops_t *ops;      /**< 当前操作函数集指针 */
    
    uint8_t key[ALGO_MAX_KEY_LEN]; /**< 密钥存储 */
    uint8_t iv[ALGO_MAX_IV_LEN];   /**< 初始化向量 */
} Algo_Context_t;

/* ==================== API 声明 ==================== */

/**
 * @brief 绑定具体算法到上下文
 * 
 * @param ctx 上下文指针
 * @param type 需要绑定的算法类型
 * @return true 绑定成功
 * @return false 不支持的类型或参数错误
 */
bool Algo_Bind(Algo_Context_t *ctx, algo_type_t type);

/**
 * @brief 设置密钥和初始化向量
 * 
 * @param ctx 上下文指针
 * @param key 密钥 (NULL则不更新)
 * @param iv 向量 (NULL则不更新)
 */
void Algo_SetKeyIV(Algo_Context_t *ctx, const uint8_t *key, const uint8_t *iv);

/**
 * @brief 执行加密 (内联封装)
 */
static inline bool Algo_Encrypt(Algo_Context_t *ctx, const uint8_t *in, uint32_t len, uint8_t *out) {
    if (ctx && ctx->ops && ctx->ops->encrypt) 
        return ctx->ops->encrypt(ctx, in, len, out);
    return false;
}

/**
 * @brief 执行解密 (内联封装)
 */
static inline bool Algo_Decrypt(Algo_Context_t *ctx, const uint8_t *in, uint32_t len, uint8_t *out) {
    if (ctx && ctx->ops && ctx->ops->decrypt) 
        return ctx->ops->decrypt(ctx, in, len, out);
    return false;
}

/**
 * @brief PKCS7 填充计算 (工具函数)
 * 
 * @param buf 数据缓冲区
 * @param data_len 原始数据长度
 * @param block_size 块大小
 * @return uint32_t 填充后的总长度
 */
uint32_t Algo_Padding(uint8_t *buf, uint32_t data_len, uint8_t block_size);

/**
 * @brief MD5 计算 (保留原有功能)
 */
void Algo_MD5_Calc(const uint8_t *in, uint32_t len, uint8_t out[16]);

#endif // EN_DE_ALGO_H
