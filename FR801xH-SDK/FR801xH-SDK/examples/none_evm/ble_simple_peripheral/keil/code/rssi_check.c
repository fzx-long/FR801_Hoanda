/**
 * @file rssi_check.c
 * @brief RSSI 信号强度滤波与判定模块
 * 
 * 算法：3 点平均 + EMA 滤波（alpha=0.3）+ 滞回判定
 * - 3 点平均：消除突发抖动
 * - EMA：平滑趋势，alpha=0.3 兼顾响应速度与平滑性
 * - 滞回：避免阈值边界频繁切换
 */

#include "rssi_check.h"
#include "co_printf.h"
#include "gap_api.h"
#include <string.h>

static RSSI_Checker g_rssi_default;

/* 业务层可选回调：用于监听距离状态变化（例如 NEAR 触发） */
static rssi_distance_change_cb_t g_distance_change_cb = NULL;

static void rssi_timer_handler(void* arg);
static void RSSI_Checker_Init_Impl(RSSI_Checker* self);
static void RSSI_Checker_Enable_Impl(RSSI_Checker* self,
                                     uint8_t       conidx,
                                     rssi_getter_t getter);
static void RSSI_Checker_Disable_Impl(RSSI_Checker* self, uint8_t conidx);
static void
RSSI_Checker_Update_Impl(RSSI_Checker* self, uint8_t conidx, int8_t rssi);
static int16_t RSSI_Checker_GetFiltered_Impl(RSSI_Checker* self,
                                             uint8_t       conidx);
static uint8_t RSSI_Checker_GetDistance_Impl(RSSI_Checker* self,
                                             uint8_t       conidx);
static bool    RSSI_Checker_IsNear_Impl(RSSI_Checker* self, uint8_t conidx);
static bool    RSSI_Checker_IsLost_Impl(RSSI_Checker* self, uint8_t conidx);
static void    RSSI_Checker_Reset_Impl(RSSI_Checker* self, uint8_t conidx);

/**
 * @brief 3 点平均 + EMA 滤波
 * @param ctx RSSI 上下文
 * @param raw 原始 RSSI 值
 * @return 滤波后的 RSSI
 */
static int8_t rssi_filter_3avg_ema(RSSI_ConnCtx* ctx, int8_t raw)
{
    if (!ctx)
    {
        return raw;
    }

    /* 3 点平均 */
    ctx->s3_buf[ctx->s3_idx] = raw;
    ctx->s3_idx              = (ctx->s3_idx + 1) % 3;
    if (ctx->s3_cnt < 3)
    {
        ctx->s3_cnt++;
    }

    int16_t sum = 0;
    for (uint8_t i = 0; i < ctx->s3_cnt; i++)
    {
        sum += ctx->s3_buf[i];
    }
    int8_t avg3 = (int8_t)(sum / ctx->s3_cnt);

    /* EMA：ema = ema + alpha * (x - ema)，alpha=0.3 */
    if (!ctx->ema_inited)
    {
        ctx->ema        = avg3;
        ctx->ema_inited = true;
    }
    else
    {
        int16_t diff = (int16_t)avg3 - ctx->ema;
        ctx->ema     = ctx->ema + (diff * 3) / 10; /* alpha=0.3 */
    }

    /* 限幅 */
    if (ctx->ema > 127)
    {
        ctx->ema = 127;
    }
    else if (ctx->ema < -127)
    {
        ctx->ema = -127;
    }

    return (int8_t)ctx->ema;
}

/**
 * @brief 滞回判定：根据滤波值更新距离状态
 * @param ctx RSSI 上下文
 */
static void rssi_hysteresis_update(RSSI_ConnCtx* ctx)
{
    RSSI_Checker*   self      = ctx ? ctx->owner : NULL;
    int16_t         val       = ctx->ema;
    rssi_distance_t old_state = ctx->distance;

    if (self == NULL)
    {
        return;
    }

    /* 根据当前状态应用不同阈值（滞回） */
    switch (ctx->distance)
    {
    case RSSI_DIST_LOST:
        if (val >
            (int16_t)self->cfg.far_threshold + (int16_t)self->cfg.hysteresis)
        {
            ctx->distance = RSSI_DIST_FAR;
        }
        break;

    case RSSI_DIST_FAR:
        if (val <
            (int16_t)self->cfg.lost_threshold - (int16_t)self->cfg.hysteresis)
        {
            ctx->distance = RSSI_DIST_LOST;
        }
        else if (val > (int16_t)self->cfg.near_threshold +
                           (int16_t)self->cfg.hysteresis)
        {
            ctx->distance = RSSI_DIST_NEAR;
        }
        break;

    case RSSI_DIST_NEAR:
        if (val <
            (int16_t)self->cfg.far_threshold - (int16_t)self->cfg.hysteresis)
        {
            ctx->distance = RSSI_DIST_FAR;
        }
        break;

    default: ctx->distance = RSSI_DIST_LOST; break;
    }

    /* 状态变化时打印 */
    if (old_state != ctx->distance)
    {
        (void)old_state;
    }
}

/**
 * @brief 定时器回调：定期读取 RSSI 并滤波/判定
 * @param arg 连接索引（uint8_t 转 void*）
 */
static void rssi_timer_handler(void* arg)
{
    RSSI_ConnCtx* ctx = (RSSI_ConnCtx*)arg;
    if (ctx == NULL || ctx->owner == NULL)
        return;
    if (!ctx->active || ctx->getter == NULL)
        return;

    uint8_t conidx = ctx->conidx;
    /* OOP：通过回调函数获取原始 RSSI（平台无关） */
    int8_t raw = ctx->getter(conidx);

    /*
     * @why
     * - 统一 RSSI 的处理入口到 RSSI_Check_Update()，这样无论 RSSI 来源是：
     *   1) GAP 事件 GAP_EVT_LINK_RSSI
     *   2) RAM 回调 gap_rssi_ind
     *   3) 轮询定时器 getter
     *   都能走同一套“滤波/滞回/状态变化回调/日志”。
     */
    RSSI_Check_Update(conidx, raw);
}

void RSSI_Checker_Construct(RSSI_Checker* self)
{
    if (self == NULL)
        return;

    memset(self, 0, sizeof(*self));

    self->m.Init        = RSSI_Checker_Init_Impl;
    self->m.Enable      = RSSI_Checker_Enable_Impl;
    self->m.Disable     = RSSI_Checker_Disable_Impl;
    self->m.Update      = RSSI_Checker_Update_Impl;
    self->m.GetFiltered = RSSI_Checker_GetFiltered_Impl;
    self->m.GetDistance = RSSI_Checker_GetDistance_Impl;
    self->m.IsNear      = RSSI_Checker_IsNear_Impl;
    self->m.IsLost      = RSSI_Checker_IsLost_Impl;
    self->m.Reset       = RSSI_Checker_Reset_Impl;

    self->cfg.near_threshold = -60;
    self->cfg.far_threshold  = -75;
    self->cfg.lost_threshold = -90;
    self->cfg.hysteresis     = 3;
    self->cfg.period_ms      = 1000;

    for (uint8_t i = 0; i < RSSI_MAX_CONN; i++)
    {
        self->conn[i].owner    = self;
        self->conn[i].conidx   = i;
        self->conn[i].ema      = -70;
        self->conn[i].distance = RSSI_DIST_LOST;

        memset(self->conn[i].peer_addr, 0, sizeof(self->conn[i].peer_addr));
        self->conn[i].peer_addr_valid = false;

        /* 定时器句柄是结构体：构造时先标记未初始化，真正启用时再 init/start */
        self->conn[i].timer_inited  = false;
        self->conn[i].timer_started = false;
    }
}

RSSI_Checker* RSSI_Get_Default(void)
{
    static bool inited = false;
    if (!inited)
    {
        RSSI_Checker_Construct(&g_rssi_default);
        g_rssi_default.m.Init(&g_rssi_default);
        inited = true;
    }
    return &g_rssi_default;
}

/**
 * @brief 初始化 RSSI 检测模块
 */
static void RSSI_Checker_Init_Impl(RSSI_Checker* self)
{
    if (self == NULL)
        return;
    for (uint8_t i = 0; i < RSSI_MAX_CONN; i++)
    {
        RSSI_ConnCtx* ctx = &self->conn[i];
        ctx->owner        = self;
        ctx->active       = false;
        ctx->conidx       = i;
        ctx->getter       = NULL;
        ctx->s3_cnt       = 0;
        ctx->s3_idx       = 0;
        ctx->ema_inited   = false;
        ctx->ema          = -70;
        ctx->distance     = RSSI_DIST_LOST;

        memset(ctx->peer_addr, 0, sizeof(ctx->peer_addr));
        ctx->peer_addr_valid = false;

        /* 停止定时器（如已启动） */
        if (ctx->timer_started)
        {
            os_timer_stop(&ctx->timer);
            ctx->timer_started = false;
        }

        /* 确保定时器已初始化（os_timer_t 不是指针，不存在 NULL） */
        if (!ctx->timer_inited)
        {
            os_timer_init(&ctx->timer, rssi_timer_handler, ctx);
            ctx->timer_inited = true;
        }
    }
}

void RSSI_Check_Init(void)
{
    RSSI_Checker* self = RSSI_Get_Default();
    self->m.Init(self);
}

/**
 * @brief 启用指定连接的 RSSI 跟踪（OOP：注入获取接口）
 * @param conidx 连接索引
 * @param getter RSSI 获取回调函数（平台相关实现）
 */
static void RSSI_Checker_Enable_Impl(RSSI_Checker* self,
                                     uint8_t       conidx,
                                     rssi_getter_t getter)
{
    if (self == NULL)
        return;
    if (conidx >= RSSI_MAX_CONN)
        return;

    RSSI_ConnCtx* ctx = &self->conn[conidx];
    ctx->owner        = self;
    ctx->active       = true;
    ctx->conidx       = conidx;
    ctx->getter       = getter;
    ctx->s3_cnt       = 0;
    ctx->s3_idx       = 0;
    ctx->ema_inited   = false;
    ctx->ema          = -70;
    ctx->distance     = RSSI_DIST_LOST;

    /* getter 为空：采用 GAP 上报/事件喂入 RSSI，不启用轮询定时器 */
    if (getter == NULL)
    {
        if (ctx->timer_started)
        {
            os_timer_stop(&ctx->timer);
            ctx->timer_started = false;
        }
        return;
    }

    /* getter 非空：启用轮询定时器 */
    if (!ctx->timer_inited)
    {
        os_timer_init(&ctx->timer, rssi_timer_handler, ctx);
        ctx->timer_inited = true;
    }
    if (!ctx->timer_started)
    {
        os_timer_start(&ctx->timer, self->cfg.period_ms, 1);
        ctx->timer_started = true;
    }
}

/*
 * GAP 实时 RSSI 上报回调（文档要求：函数名与参数固定，且放在 RAM）
 * 注意：此回调中不要做过多操作，只做“喂入 + 滤波”。
 */
__attribute__((section("ram_code"))) void gap_rssi_ind(int8_t  rssi,
                                                       uint8_t conidx)
{
    if (conidx >= RSSI_MAX_CONN)
    {
        return;
    }

    /*
     * @why
     * - 文档强调该 RAM 回调里不要做复杂工作。
     * - 这里仅“喂入”到统一入口，滤波/滞回/回调/打印都在 RSSI_Check_Update() 内完成。
     */
    RSSI_Check_Update(conidx, rssi);
}

void RSSI_Check_Set_DistanceChangeCb(rssi_distance_change_cb_t cb)
{
    g_distance_change_cb = cb;
}

void RSSI_Check_Set_Peer_Addr(uint8_t conidx, const uint8_t* addr6)
{
    if (conidx >= RSSI_MAX_CONN || addr6 == NULL)
    {
        return;
    }
    RSSI_Checker* self = RSSI_Get_Default();
    RSSI_ConnCtx* ctx  = &self->conn[conidx];
    memcpy(ctx->peer_addr, addr6, 6);
    ctx->peer_addr_valid = true;
}

bool RSSI_Check_Get_Peer_Addr(uint8_t conidx, uint8_t* out_addr6)
{
    if (conidx >= RSSI_MAX_CONN || out_addr6 == NULL)
    {
        return false;
    }

    RSSI_Checker* self = RSSI_Get_Default();
    RSSI_ConnCtx* ctx  = &self->conn[conidx];
    if (!ctx->peer_addr_valid)
    {
        return false;
    }

    memcpy(out_addr6, ctx->peer_addr, 6);
    return true;
}

void RSSI_Check_Clear_Peer_Addr(uint8_t conidx)
{
    if (conidx >= RSSI_MAX_CONN)
    {
        return;
    }
    RSSI_Checker* self = RSSI_Get_Default();
    RSSI_ConnCtx* ctx  = &self->conn[conidx];
    memset(ctx->peer_addr, 0, sizeof(ctx->peer_addr));
    ctx->peer_addr_valid = false;
}

void RSSI_Check_Enable(uint8_t conidx, rssi_getter_t getter)
{
    RSSI_Checker* self = RSSI_Get_Default();
    self->m.Enable(self, conidx, getter);
}

/**
 * @brief 禁用指定连接的 RSSI 跟踪
 * @param conidx 连接索引
 */
static void RSSI_Checker_Disable_Impl(RSSI_Checker* self, uint8_t conidx)
{
    if (self == NULL)
        return;
    if (conidx >= RSSI_MAX_CONN)
        return;
    RSSI_ConnCtx* ctx = &self->conn[conidx];

    if (ctx->timer_started)
    {
        os_timer_stop(&ctx->timer);
        ctx->timer_started = false;
    }

    ctx->active = false;
    ctx->getter = NULL;
}

void RSSI_Check_Disable(uint8_t conidx)
{
    RSSI_Checker* self = RSSI_Get_Default();
    self->m.Disable(self, conidx);
}

/**
 * @brief 手动更新 RSSI（若不用定时器，可外部调用此接口喂值）
 * @param conidx 连接索引
 * @param rssi   原始 RSSI 值（负数，单位 dBm）
 */
static void
RSSI_Checker_Update_Impl(RSSI_Checker* self, uint8_t conidx, int8_t rssi)
{
    if (self == NULL)
        return;
    if (conidx >= RSSI_MAX_CONN)
        return;
    RSSI_ConnCtx* ctx = &self->conn[conidx];
    if (!ctx->active)
        return;

    /* 记录变化前的状态：用于边沿触发（NEAR/FAR/LOST） */
    uint8_t prev_distance = (uint8_t)ctx->distance;

    (void)rssi_filter_3avg_ema(ctx, rssi);
    rssi_hysteresis_update(ctx);

    /* 状态变化通知（业务层可选注册） */
    uint8_t new_distance = (uint8_t)ctx->distance;
    if ((g_distance_change_cb != NULL) && (new_distance != prev_distance))
    {
        g_distance_change_cb(conidx, new_distance, (int16_t)ctx->ema, rssi);
    }

#if RSSI_PRINT_ENABLE
    /*
     * @why
     * - 你要求“必须能看到 RSSI 值”，所以把打印放到统一入口，确保 GAP_EVT_LINK_RSSI 也能看到。
     * - 打印 raw/filtered/distance/mac，方便你对照 MCU 是否收到 0x0118。
     */
    if (ctx->peer_addr_valid)
    {
        co_printf("[RSSI] link=%d raw=%d flt=%d dist=%u "
                  "mac=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                  conidx,
                  rssi,
                  (int)ctx->ema,
                  (unsigned)ctx->distance,
                  ctx->peer_addr[0],
                  ctx->peer_addr[1],
                  ctx->peer_addr[2],
                  ctx->peer_addr[3],
                  ctx->peer_addr[4],
                  ctx->peer_addr[5]);
    }
    else
    {
        co_printf("[RSSI] link=%d raw=%d flt=%d dist=%u mac=UNKNOWN\r\n",
                  conidx,
                  rssi,
                  (int)ctx->ema,
                  (unsigned)ctx->distance);
    }
#endif
}

void RSSI_Check_Update(uint8_t conidx, int8_t rssi)
{
    RSSI_Checker* self = RSSI_Get_Default();
    self->m.Update(self, conidx, rssi);
}

/**
 * @brief 获取指定连接的滤波后 RSSI
 * @param conidx 连接索引
 * @return 滤波后 RSSI（dBm）
 */
static int16_t RSSI_Checker_GetFiltered_Impl(RSSI_Checker* self, uint8_t conidx)
{
    if (self == NULL)
        return -100;
    if (conidx >= RSSI_MAX_CONN)
        return -100;
    return self->conn[conidx].ema;
}

int16_t RSSI_Check_Get_Filtered(uint8_t conidx)
{
    RSSI_Checker* self = RSSI_Get_Default();
    return self->m.GetFiltered(self, conidx);
}

/**
 * @brief 获取指定连接的距离状态
 * @param conidx 连接索引
 * @return 0=失联，1=远距离，2=近距离
 */
static uint8_t RSSI_Checker_GetDistance_Impl(RSSI_Checker* self, uint8_t conidx)
{
    if (self == NULL)
        return 0;
    if (conidx >= RSSI_MAX_CONN)
        return 0;
    return (uint8_t)self->conn[conidx].distance;
}

uint8_t RSSI_Check_Get_Distance(uint8_t conidx)
{
    RSSI_Checker* self = RSSI_Get_Default();
    return self->m.GetDistance(self, conidx);
}

/**
 * @brief 判断是否近距离
 * @param conidx 连接索引
 * @return true=近距离，false=其他
 */
static bool RSSI_Checker_IsNear_Impl(RSSI_Checker* self, uint8_t conidx)
{
    if (self == NULL)
        return false;
    if (conidx >= RSSI_MAX_CONN)
        return false;
    return self->conn[conidx].distance == RSSI_DIST_NEAR;
}

bool RSSI_Check_Is_Near(uint8_t conidx)
{
    RSSI_Checker* self = RSSI_Get_Default();
    return self->m.IsNear(self, conidx);
}

/**
 * @brief 判断是否失联
 * @param conidx 连接索引
 * @return true=失联，false=其他
 */
static bool RSSI_Checker_IsLost_Impl(RSSI_Checker* self, uint8_t conidx)
{
    if (self == NULL)
        return false;
    if (conidx >= RSSI_MAX_CONN)
        return false;
    return self->conn[conidx].distance == RSSI_DIST_LOST;
}

bool RSSI_Check_Is_Lost(uint8_t conidx)
{
    RSSI_Checker* self = RSSI_Get_Default();
    return self->m.IsLost(self, conidx);
}

/**
 * @brief 重置指定连接的滤波器
 * @param conidx 连接索引
 */
static void RSSI_Checker_Reset_Impl(RSSI_Checker* self, uint8_t conidx)
{
    if (self == NULL)
        return;
    if (conidx >= RSSI_MAX_CONN)
        return;
    RSSI_ConnCtx* ctx = &self->conn[conidx];
    ctx->s3_cnt       = 0;
    ctx->s3_idx       = 0;
    ctx->ema_inited   = false;
    ctx->ema          = -70;
    ctx->distance     = RSSI_DIST_LOST;
}

void RSSI_Check_Reset(uint8_t conidx)
{
    RSSI_Checker* self = RSSI_Get_Default();
    self->m.Reset(self, conidx);
}
