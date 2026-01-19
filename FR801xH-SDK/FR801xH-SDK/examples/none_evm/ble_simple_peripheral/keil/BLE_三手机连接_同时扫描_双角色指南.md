# FR801xH：三手机同时连接 + SOC 同时作为主机扫描（双角色 Multi-Role）指南

适用工程：`examples/none_evm/ble_simple_peripheral`

你现在的目标是：
1) **3 个手机同时连接 SOC**（手机=Central，SOC=Peripheral/Slave）
2) SOC 在保持 3 条从机连接的同时，还要 **作为主机(Central/Master)去扫描** 周围设备（必要时还能发起连接）

> 术语提醒：
> - 在 BLE 里，**“主/从”是针对每一条连接(link)而言**。
> - 同一个 SOC 可以在一些 link 上当 **Slave(peripheral)**（被手机连），同时在另一些操作上当 **Master(central)**（扫描/主动连别人）。这叫 **Multi-Role**。

---

## 1) 你这个 SDK 里对应的 API / 事件（关键点）

### 1.1 多连接数量
SDK 提供：

```c
uint8_t gap_get_connect_num(void);
```

用于查询当前活跃连接数。

### 1.2 扫描（作为主机侧能力）
SDK 提供：

```c
typedef struct {
    uint8_t  scan_mode;    // GAP_SCAN_MODE_GEN_DISC / GAP_SCAN_MODE_OBSERVER
    uint8_t  dup_filt_pol; // 0 不过滤重复; 1 过滤
    uint8_t  phy_mode;     // GAP_PHY_1MBPS / GAP_PHY_2MBPS / GAP_PHY_CODED
    uint16_t scan_intv;    // 625us 单位
    uint16_t scan_window;  // 625us 单位, <= scan_intv
    uint16_t duration;     // 10ms 单位, 0=一直扫直到 gap_stop_scan
} gap_scan_param_t;

void gap_start_scan(gap_scan_param_t *p_scan_param);
void gap_stop_scan(void);
```

扫描结果上报事件：

- `GAP_EVT_ADV_REPORT`：每收到一个广播/扫描回应就会上报
- `GAP_EVT_SCAN_END`：扫描结束（如果你设置了 duration）

扫描上报数据结构：

```c
typedef struct {
    uint8_t        evt_type;  // 广播类型位字段
    gap_mac_addr_t src_addr;  // 对端地址 + addr_type
    int8_t         tx_pwr;
    int8_t         rssi;
    uint16_t       length;
    uint8_t       *data;      // Adv/ScanRsp 原始数据
} gap_evt_adv_report_t;
```

---

## 2) 三手机连接：SOC 应该是从机吗？

是的：
- 手机（nRF Connect / 系统蓝牙）默认是 **Central**。
- 你的 SOC 作为 **Peripheral/Slave** 广播、被连接。

你工程里现有的 GAP 回调 `app_gap_evt_cb()`（位于 `code/ble_simple_peripheral.c`）已经在处理：
- `GAP_EVT_SLAVE_CONNECT`
- `GAP_EVT_DISCONNECT`

这说明工程默认就是“外设(从机)模式”。

---

## 3) 三手机同时连接：需要改什么逻辑？

### 3.1 关键点：每连上一个手机后，要继续广播
BLE 外设在 **连接建立后会停止广播**（这是正常行为）。

要支持 3 手机同时连，你需要在 **每次收到 `GAP_EVT_SLAVE_CONNECT` 之后**：
- 判断当前已连接数 `< 3`，就再次 `gap_start_advertising(0)` 继续让第 2/第 3 个手机能搜到并连接。

### 3.2 建议写法（示例伪码）
放在 `app_gap_evt_cb()` 的 `GAP_EVT_SLAVE_CONNECT` 分支里：

```c
#define MAX_PHONE_LINK 3

case GAP_EVT_SLAVE_CONNECT:
{
    uint8_t conidx = p_event->param.slave_connect.conidx;

    // 记录 conidx（推荐：用数组保存 3 个 conidx，后续发 notify 需要）

    if (gap_get_connect_num() < MAX_PHONE_LINK)
    {
        // 继续广播，让下一台手机还能连接
        gap_start_advertising(0);
    }
}
break;

case GAP_EVT_DISCONNECT:
{
    // 删除对应 conidx 记录

    if (gap_get_connect_num() < MAX_PHONE_LINK)
    {
        // 有空位就继续广播
        gap_start_advertising(0);
    }
}
break;
```

> 注意：你当前工程在 `GAP_EVT_DISCONNECT` 已经会 `gap_start_advertising(0)`，所以主要是补上“连接后继续广播”的逻辑。

### 3.3 是否一定能连 3 台？
取决于 BLE 栈的“最大连接数”配置与 RAM 资源。
- 你工程里 `gap_bond_manager_init(..., 8, ...)` 只是“最多存 8 份绑定信息”，不等于“最多连接 8 台”。
- 实际最大连接数如果小于 3，第 3 台手机会连不上，或连接后很快断开。

判断方式：
- 串口打印 `gap_get_connect_num()`
- 看是否有 `GAP_EVT_SLAVE_CONNECT` 第 3 次出现

---

## 4) SOC 同时作为主机去扫描：怎么做？

### 4.1 在哪里启动扫描
建议策略（最稳）：
- **先连满 3 台手机**（或至少连接稳定后），再启动扫描。
- 或者用定时器“间歇扫描”：比如每 10 秒扫 2 秒。

原因：扫描会占用射频时间片，过大 scan_window/持续扫描可能影响连接链路的稳定性。

### 4.2 扫描参数推荐
示例（比较保守）：

```c
gap_scan_param_t sp;
sp.scan_mode    = GAP_SCAN_MODE_GEN_DISC;
sp.dup_filt_pol = 1;              // 过滤重复，减少上报压力
sp.phy_mode     = GAP_PHY_1MBPS;
sp.scan_intv    = 160;            // 160 * 0.625ms = 100ms
sp.scan_window  = 32;             // 32  * 0.625ms = 20ms
sp.duration     = 200;            // 200 * 10ms = 2s

gap_start_scan(&sp);
```

停止扫描：

```c
gap_stop_scan();
```

### 4.3 在 GAP 回调里接收扫描结果
在 `app_gap_evt_cb()` 里新增：

```c
case GAP_EVT_ADV_REPORT:
{
    gap_evt_adv_report_t *r = p_event->param.adv_rpt;

    // r->src_addr.addr.addr[0..5] 是 MAC
    // r->src_addr.addr_type 是地址类型
    // r->rssi 是信号强度
    // r->data / r->length 是 Adv/ScanRsp 原始数据

    // 你可以在这里筛选：例如按名称/按 Service UUID/按厂家数据
}
break;

case GAP_EVT_SCAN_END:
{
    // 扫描结束（如果 duration != 0）
}
break;
```

---

## 5) （可选）扫描到设备后，作为主机去连接

SDK 提供：

```c
void gap_start_conn(mac_addr_t *addr,
                   uint8_t addr_type,
                   uint16_t min_itvl,
                   uint16_t max_itvl,
                   uint16_t slv_latency,
                   uint16_t timeout);
```

你可以在 `GAP_EVT_ADV_REPORT` 里筛选到目标设备后调用它。

连接建立后会收到：
- `GAP_EVT_MASTER_CONNECT`

这条连接上 SOC 就是 **Master**。

---

## 6) 重要注意事项（避免“连着连着就断/扫不出东西”）

1. **不要把 scan_window 开太大、也不要一直扫**：
   - 建议用“短时扫描 + 间隔扫描”。
2. **多连接时 conidx 会有多个**：
   - 发通知/发数据时要用正确的 conidx。
3. **广播与扫描并发**：
   - 一些 BLE Controller 在“正在广播”时也能扫描，但会更吃时隙；更稳的是“先连满 3 个，再扫描”。

---

## 7) 你下一步可以怎么做（建议动作）

- 第一步：先把“连接后继续广播”补上，验证能稳定连上 3 台手机（看 `gap_get_connect_num()`）。
- 第二步：在连接稳定后，增加一个定时器，周期性 `gap_start_scan()`（duration=2s）并在 `GAP_EVT_ADV_REPORT` 打印 RSSI+MAC。

如果你告诉我：
- 你要扫描的目标设备特征（名字/Service UUID/厂家数据）
- 以及 3 台手机连接时希望的连接间隔/吞吐需求

我可以再给你一份“扫描过滤规则 + 最小化对 3 连接影响的参数组合”。
