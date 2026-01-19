# FR801xH ble_simple_peripheral：自定义 UUID / Profile 与 nRF Connect 收发数据指南

适用工程：`examples/none_evm/ble_simple_peripheral`

这份文档基于你当前工程的真实实现方式：
- 应用入口在 `code/ble_simple_peripheral.c` 的 `simple_peripheral_init()`
- GATT Profile/Service 示例实现位于 `components/ble/profiles/ble_simple_profile/simple_gatt_service.c/.h`

---

## 1. 先理解：这个工程里“Profile/Service/UUID”是怎么组织的

在本 SDK 里，你创建“Profile”本质上就是：
1. 定义一个 **Service UUID**（16-bit 或 128-bit）。
2. 在一个 **attribute table**（属性表）里声明该 Service 下的所有 Characteristic（特征值）与 Descriptor（描述符）。
3. 注册一个 **GATT 消息回调**，在回调里处理手机的 Read/Write 请求，并在需要时发 Notification。
4. 调用 `gatt_add_service()` 把该 Service 加进 ATT 数据库。

本工程的示例 Service：
- Service UUID：`SP_SVC_UUID = 0xFFF0`
- Characteristic：共 5 个（`SP_IDX_CHAR1 ~ SP_IDX_CHAR5`）
- 其中：
  - Char1：`Notify`（设备 -> 手机）使用 **128-bit UUID**：`SP_CHAR1_TX_UUID`
  - Char2：`Write`（手机 -> 设备）使用 **128-bit UUID**：`SP_CHAR2_RX_UUID`
  - Char3/4/5：使用 16-bit UUID（0xFFF3/0xFFF4/0xFFF5）

---

## 2. UUID 怎么创建？（16-bit vs 128-bit）

### 2.1 16-bit UUID（开发调试最简单）

你工程里 Service 就是 16-bit：

```c
#define SP_SVC_UUID  0xFFF0

const uint8_t sp_svc_uuid[] = UUID16_ARR(SP_SVC_UUID);
```

Characteristic 也可以用 16-bit：

```c
#define SP_CHAR4_UUID 0xFFF4

{ UUID_SIZE_2, UUID16_ARR(SP_CHAR4_UUID) }
```

> 说明：0xFFF0~0xFFFF 常被厂商自定义用于演示/调试；如果是正式产品，建议使用 128-bit UUID 避免冲突。

### 2.2 128-bit UUID（正式更推荐）

你工程里 Char1/Char2 是 128-bit：

```c
#define SP_CHAR1_TX_UUID {0xb8,0x5c,0x49,0xd2,0x04,0xa3,0x40,0x71,0xa0,0xb5,0x35,0x85,0x3e,0xb0,0x83,0x07}
#define SP_CHAR2_RX_UUID {0xba,0x5c,0x49,0xd2,0x04,0xa3,0x40,0x71,0xa0,0xb5,0x35,0x85,0x3e,0xb0,0x83,0x07}

// Attribute table 里使用：
{ UUID_SIZE_16, SP_CHAR1_TX_UUID }
{ UUID_SIZE_16, SP_CHAR2_RX_UUID }
```

> 注意：这里数组顺序是“字节数组顺序”，不需要你再做大小端转换。

---

## 3. Profile（Service + Characteristic）怎么创建/新增？

下面按“在现有 simple_gatt_service 基础上新增一个特征值”为例（最贴近你现在的工程）。

### 3.1 增加一个新 Characteristic 的最小步骤

目标：新增 Char6，支持 `Write`（手机写入）+ `Notify`（设备通知）。

#### (1) 在 `simple_gatt_service.h` 的索引 enum 里加索引

示例（思路）：
- `SP_IDX_CHAR6_DECLARATION`
- `SP_IDX_CHAR6_VALUE`
- 如果要 Notify/Indicate，还要 `SP_IDX_CHAR6_CFG`（CCC 描述符）
- 可选加 `SP_IDX_CHAR6_USER_DESCRIPTION`

最后别忘了让 `SP_IDX_NB` 变大（它代表属性表项数）。

#### (2) 在 `simple_gatt_service.c` 定义 Char6 的 UUID/长度/缓存

示例：

```c
#define SP_CHAR6_UUID 0xFFF6
#define SP_CHAR6_VALUE_LEN 20
static uint8_t sp_char6_value[SP_CHAR6_VALUE_LEN];
```

如果你要 128-bit UUID，就参考 Char1/Char2 的写法。

#### (3) 在 `simple_profile_att_table[]` 加 attribute 定义

一个典型“支持 Notify 的特征值”至少 4 项：
1) Declaration
2) Value
3) CCC（Client Characteristic Configuration）
4) User Description（可选）

示例（关键点：Value 的 Permissions 要带 `GATT_PROP_NOTI`，CCC 要用 `GATT_CLIENT_CHAR_CFG_UUID`）：

```c
// Char6 Declaration
[SP_IDX_CHAR6_DECLARATION] = {
  { UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID) },
  GATT_PROP_READ,
  0,
  NULL,
},

// Char6 Value
[SP_IDX_CHAR6_VALUE] = {
  { UUID_SIZE_2, UUID16_ARR(SP_CHAR6_UUID) },
  GATT_PROP_READ | GATT_PROP_WRITE | GATT_PROP_NOTI,
  SP_CHAR6_VALUE_LEN,
  NULL,
},

// Char6 CCC
[SP_IDX_CHAR6_CFG] = {
  { UUID_SIZE_2, UUID16_ARR(GATT_CLIENT_CHAR_CFG_UUID) },
  GATT_PROP_READ | GATT_PROP_WRITE,
  2,
  NULL,
},
```

#### (4) 在 `sp_gatt_msg_handler()` 里处理写入/读取/CCC

本工程里所有手机写入都会走：

```c
case GATTC_MSG_WRITE_REQ:
```

你可以照 Char2 的写法增加：

```c
else if (p_msg->att_idx == SP_IDX_CHAR6_VALUE) {
  // p_msg->param.msg.p_msg_data / msg_len 就是手机写入的数据
}
else if (p_msg->att_idx == SP_IDX_CHAR6_CFG) {
  // 手机点开 Notify 后，会写 CCC=0x0001 到这里
}
```

---

## 4. 设备 -> 手机：怎么“发送数据”到 nRF Connect？（Notification）

### 4.1 先明确：发送通道是哪一个 Characteristic？

你工程里最直接的“发送通道”是 Char1：
- `SP_IDX_CHAR1_VALUE` 具备 `GATT_PROP_NOTI`
- 手机打开通知后，写入 `SP_IDX_CHAR1_CFG`，代码里把 `ntf_char1_enable=1` 并发送一次测试通知

发送函数已经在 `simple_gatt_service.c` 提供了：

```c
void ntf_data(uint8_t con_idx, uint8_t att_idx, uint8_t *data, uint16_t len)
{
  gatt_ntf_t ntf_att;
  ntf_att.att_idx = att_idx;
  ntf_att.conidx = con_idx;
  ntf_att.svc_id = sp_svc_id;
  ntf_att.data_len = len;
  ntf_att.p_data = data;
  gatt_notification(ntf_att);
}
```

所以你只需要在合适的时机调用：

```c
if (ntf_char1_enable) {
  ntf_data(conidx, SP_IDX_CHAR1_VALUE, (uint8_t*)payload, payload_len);
}
```

### 4.2 conidx 从哪里来？

连接建立时，GAP 回调 `app_gap_evt_cb()` 会收到：

```c
case GAP_EVT_SLAVE_CONNECT:
  // p_event->param.slave_connect.conidx
```

你可以把 `conidx` 保存成全局变量，后续按键/定时器/串口事件里用它发送。

### 4.3 数据长度注意事项

- Notification 单包最大有效负载通常是 `MTU - 3`。
- 工程里也会收到 `GAP_EVT_MTU`，可以用它决定分包策略。

---

## 5. 手机 -> 设备：怎么从 nRF Connect “写数据”给设备？（Write）

你的工程里最直接的“接收通道”是 Char2（128-bit）：
- Attribute index：`SP_IDX_CHAR2_VALUE`
- 属性：`GATT_PROP_READ | GATT_PROP_WRITE`

当手机写入后，会走到 `sp_gatt_msg_handler()`：

```c
case GATTC_MSG_WRITE_REQ:
  else if (p_msg->att_idx == SP_IDX_CHAR2_VALUE) {
    // 这里就是手机发来的数据
    // p_msg->param.msg.p_msg_data  指向数据
    // p_msg->param.msg.msg_len     数据长度
  }
```

当前示例代码做的是打印：

```c
co_printf("char2_recv:");
show_reg(p_msg->param.msg.p_msg_data, p_msg->param.msg.msg_len, 1);
```

你要做业务处理的话，就在这里解析协议即可（比如首字节为命令、后面为 payload）。

---

## 6. nRF Connect（手机端）如何测试收发？

以 Char1/Char2 为例（最典型的一收一发）：

1. 打开 nRF Connect，扫描并连接名为 `Simple Peripheral` 的设备。
2. 进入 Services：找到 `0xFFF0` 对应的 Service（或在 Unknown Service 里看特征值 UUID）。
3. **接收设备数据（Notify）**：
   - 找到 Char1（UUID 以 `b85c...8307` 开头的那条）
   - 点击 `Notify` 开关（或铃铛图标）
   - 你会看到设备立刻发一次 `char1_ntf_data`（工程里在写 CCC 后就发了）
4. **发送数据到设备（Write）**：
   - 找到 Char2（UUID 以 `ba5c...8307` 开头的那条）
   - 点 `Write` / `Write without response`，选择 Text 或 Hex
   - 写入后串口日志会打印 `char2_recv:` + 十六进制字节

---

## 7. 常见坑（你一旦收不到/发不出，优先看这里）

1) **Notify 没开 CCC**：手机必须先对 CCC 写入 `0x0001`，设备才能发通知。
2) **conidx 不对**：多连接时必须用正确 `conidx`。
3) **广播里 Service UUID 不一致**：
   - 你工程的 `adv_data` 当前广播的是 `0xFEFF`（示例值），不是 `0xFFF0`。
   - 如果想让 nRF Connect 更容易识别，可以把 `adv_data` 改为 `SP_SVC_UUID`。
4) **长度超过 MTU**：Notify/Write 分包或限制长度。

---

## 8. 你要“最常用的收发模板”

- 设备发送（Notify）：在任意位置调用（前提：已连接且 CCC=1）

```c
extern uint8_t ntf_char1_enable;

void app_send_to_phone(uint8_t conidx, const uint8_t *buf, uint16_t len)
{
  if (!ntf_char1_enable) return;
  ntf_data(conidx, SP_IDX_CHAR1_VALUE, (uint8_t*)buf, len);
}
```

- 设备接收（Write）：在 `sp_gatt_msg_handler()` 里处理

```c
else if (p_msg->att_idx == SP_IDX_CHAR2_VALUE) {
  // buf = p_msg->param.msg.p_msg_data
  // len = p_msg->param.msg.msg_len
}
```

如果你告诉我：你希望“发/收的数据格式是什么”（比如 JSON、AT 命令、二进制协议），我可以基于当前工程直接给你一套完整的协议解析与发送封装（含分包/MTU 处理）。
