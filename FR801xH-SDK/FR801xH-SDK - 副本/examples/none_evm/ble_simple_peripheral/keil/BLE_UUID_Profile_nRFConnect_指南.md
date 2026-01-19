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

---

## 9. 协议：命令/开关清单 + 基本数据定义（与代码一致）

你这份工程里其实有两套“命令体系”，它们经常会被混在一起讨论，导致联调时“写对了特征值但命令不生效 / 回包看不懂 / 长度算错”的问题：

- BLE（手机↔设备）协议：在 `protocol.h` + `code/protocol.c`，帧头 `0x5555`、帧尾 `0xAAAA`，Cmd 是 16-bit。
- SOC↔MCU 串口协议：在 `code/usart_cmd.h`，帧头 `0xFE`、帧尾 `0x0A 0x0D`，命令用 `CMD_*` 定义。

把这两套在文档里固定成“字段定义 + 命令表”，是为了让后续新增命令/排查问题时：
1) 先确认“哪套协议/哪条通道”，2) 再按表核对字段长度/端序/校验范围，3) 最后才去看业务处理函数。

### 9.1 BLE（手机↔设备）二进制帧格式（`protocol.h` / `code/protocol.c`）

帧结构如下（最小长度 10 字节）：

| 字段 | 长度 | 说明 |
|---|---:|---|
| Header | 2B | 固定 `0x5555`（字节序列 `55 55`） |
| Length | 1B | 包总长度（含 Header/BCC/Footer），必须等于实际收到的字节数 |
| Crypto | 1B | 加密类型：`0x00`=NONE，`0x01`=DES3，`0x02`=AES128 |
| Seq | 1B | 流水号/序号（用于 ACK/重传时匹配） |
| Cmd | 2B | 命令 ID（在 ARM Cortex-M3 上按小端解释；低字节用来区分 `0xFE/0xFD`） |
| Data | N | 载荷 |
| BCC | 1B | XOR 校验：从 Header 开始到 Data 末尾（不包含 BCC 与 Footer） |
| Footer | 2B | 固定 `0xAAAA`（字节序列 `AA AA`） |

### 9.2 SOC↔MCU 串口帧格式（`code/usart_cmd.h`）

当前头文件里的约定是：

| 字段 | 长度 | 说明 |
|---|---:|---|
| head | 1B | 固定 `0xFE` |
| sync | 2B | `0xABAB`=SOC→MCU，下发；`0xBABA`=MCU→SOC，上报/回包 |
| feature | 2B | 通道字段：`0xFF01~0xFF04`（按小端解析：字节 `01 FF` 表示 `0xFF01`） |
| len | 2B | 小端；头文件写法是 `len = id(1B) + data_len` |
| id | 1B（按注释） | 命令 ID（`CMD_*`） |
| data | N | 载荷 |
| crc | 1B/2B | 由 `SOC_MCU_USE_CRC16` 决定；默认 CRC16（2B，小端输出） |
| end | 2B | 固定 `0x0A 0x0D` |

注意（这是一个非常容易导致“命令不生效/解析错命令”的根因）：

- `usart_cmd.h` 的注释写的是 `id(1B)`，但实际 `CMD_HDC_switch_on` 等命令值是 `0x100~0x116`（已经超过 1 字节）。
- 如果你的串口解析层按 **1 字节**读 id，那么 `0x100` 会被截断成 `0x00`，命令就会错乱。

建议你们在协议层统一二选一：
1) **id=1B**：所有 `CMD_*` 必须 `<= 0xFF`（把 `0x100` 这类值修正为 8-bit 范围）；
2) **id=2B**：`id` 改成 2 字节小端，同时 `len` 也要改为 `2B + data_len`。

### 9.3 基本数据定义（双方约定写在这里，避免“口口相传”）

| 名称 | 长度 | 编码/端序 | 说明 |
|---|---:|---|---|
| u8 | 1B | 无 | 0~255 |
| s8 | 1B | 补码 | -128~127 |
| u16_le | 2B | 小端 | 低字节在前 |
| s16_le | 2B | 小端补码 | -32768~32767 |
| bool | 1B | `0x00`=false，其它= true（建议只用 `0x00/0x01`） | 开关量 |
| mac6 | 6B | 原始字节序列 | MAC 地址 6 字节（不要再做大小端“翻转”） |
| bytes | N | 原始 | 二进制块 |
| BCC(XOR) | 1B | XOR | BLE 帧里使用（见 9.1） |
| CRC16 | 2B | 小端输出 | 串口帧默认使用（见 9.2） |

### 9.4 命令与开关清单（从头文件抽取）

来源：
- BLE 命令：`code/protocol_cmd.h`
- SOC↔MCU 命令：`code/usart_cmd.h`

#### 9.4.1 BLE（APP 下行，手机→设备，Cmd 低字节=0xFE）

| 名称 | ID | 方向 | 说明 |
|---|---|---|---|
| connect_ID | 0x01FE | 手机→设备 | 连接命令 |
| defences_ID | 0x03FE | 手机→设备 | 布防命令 |
| anti_theft_ID | 0x04FE | 手机→设备 | 防盗命令 |
| phone_message_ID | 0x05FE | 手机→设备 | 来电短信命令 |
| riss_strength_ID | 0x07FE | 手机→设备 | 信号强度命令 |
| car_search_ID | 0x08FE | 手机→设备 | 寻车命令 |
| factory_settings_ID | 0x09FE | 手机→设备 | 恢复出厂设置命令 |
| ble_unpair_ID | 0x0AFE | 手机→设备 | 解绑命令 |
| add_nfc_ID | 0x0BFE | 手机→设备 | 添加NFC命令 |
| delete_nfc_ID | 0x0CFE | 手机→设备 | 删除NFC命令 |
| search_nfc_ID | 0x0EFE | 手机→设备 | 查询NFC命令 |
| oil_defence_ID | 0x0FFE | 手机→设备 | 油电防护命令 |
| oil_car_search_ID | 0x11FE | 手机→设备 | 油电寻车命令 |
| set_boot_lock_ID | 0x12FE | 手机→设备 | 设置尾箱锁命令 |
| set_nfc_ID | 0x13FE | 手机→设备 | 设置NFC命令 lock |
| set_seat_lock_ID | 0x14FE | 手机→设备 | 设置座椅锁命令 |
| set_car_mute_ID | 0x15FE | 手机→设备 | 设置车辆静音命令 |
| set_mid_box_lock_ID | 0x16FE | 手机→设备 | 设置中箱锁命令 |
| set_emergency_mode_ID | 0x17FE | 手机→设备 | 紧急模式命令 |
| get_phone_mac_ID | 0x18FE | 手机→设备 | 获取手机MAC地址命令 |
| set_unlock_mode_ID | 0x19FE | 手机→设备 | 设置单控开锁（注释：待确认） |

#### 9.4.2 BLE（智能设置，手机→设备，Cmd 低字节=0xFD）

| 名称 | ID | 方向 | 说明 |
|---|---|---|---|
| assistive_trolley | 0x02FD | 手机→设备 | 智能助力推车 |
| delayed_headlight | 0x03FD | 手机→设备 | 延时大灯 |
| set_charging_power | 0x04FD | 手机→设备 | 充电功率 |
| set_p_gear_mode | 0x05FD | 手机→设备 | 设置P档模式 |
| set_chord_horn_mode | 0x06FD | 手机→设备 | 设置和弦模式 |
| set_RGB_light_mode | 0x07FD | 手机→设备 | 设置RGB氛围灯模式 |
| set_auxiliary_parking | 0x08FD | 手机→设备 | 设置辅助倒车 |
| set_vichle_gurd_mode | 0x09FD | 手机→设备 | 设置车辆防盗 |
| set_auto_return_mode | 0x0AFD | 手机→设备 | 设置自动归位 |
| set_EBS_switch | 0x0BFD | 手机→设备 | 设置EBS开关 |
| set_E_SAVE_mode | 0x0CFD | 手机→设备 | 设置E节能模式（低速挡） |
| set_DYN_mode | 0x0DFD | 手机→设备 | 设置DYN模式（中速档） |
| set_sport_mode | 0x0EFD | 手机→设备 | 设置SPORT模式（高速档） |
| set_lost_mode | 0x0FFD | 手机→设备 | 设置丢失模式 |
| set_TCS_switch | 0x10FD | 手机→设备 | 设置TCS开关 |
| set_side_stand | 0x11FD | 手机→设备 | 设置侧撑开关 |
| set_battery_parameter | 0x12FD | 手机→设备 | 设置电池参数 |
| set_updata_APP | 0x13FD | 手机→设备 | 数据同步（连接后每隔30秒发送一次） |
| set_HDC_mode | 0x14FD | 手机→设备 | 设置HDC模式 |
| set_HHC_mode | 0x15FD | 手机→设备 | 设置HHC模式 |
| set_start_ability | 0x16FD | 手机→设备 | 设置启动能力 |
| set_sport_power_speed | 0x17FD | 手机→设备 | 设置sport动力速度 |
| set_ECO_mode | 0x18FD | 手机→设备 | 设置ECO模式 |
| set_radar_switch | 0x19FD | 手机→设备 | 设置雷达开关 |
| set_intelligent_switch | 0x63FD | 手机→设备 | 设置智能开关 |
| paramter_synchronize | 0x64FD | 手机→设备 | 参数同步（每次连接后同步一次） |
| set_default_mode | 0x65FD | 手机→设备 | 设置默认模式 |
| paramter_synchronize_change | 0x66FD | 手机→设备 | 参数同步变化（参数变化后立即同步） |

#### 9.4.3 BLE（设备→手机）

| 名称 | ID | 方向 | 说明 |
|---|---|---|---|
| auth_result_ID | 0x0101 | 设备→手机 | 鉴权结果回复（Connect 0x01FE 的应答） |

#### 9.4.4 SOC↔MCU（串口）命令与开关（`CMD_*`）

说明：方向通常是 SOC→MCU（控制/设置），MCU→SOC 的回包/上报是否复用同一 ID，需要你们在串口协议层约定。

| 名称 | ID | 说明 |
|---|---|---|
| CMD_CONNECT | 0x01 | 蓝牙连接命令（鉴权完成后 SOC 主动发给 MCU） |
| CMD_BLE_FACTORY_RESET | 0x16 | 蓝牙恢复出厂设置 |
| CMD_BLE_CANCEL_PAIRING | 0x17 | 取消蓝牙配对 |
| CMD_ADD_NFC_KEY | 0x20 | 添加NFC钥匙（与模块相关） |
| CMD_DELETE_NFC_KEY | 0x21 | 删除NFC钥匙 |
| CMD_FIND_NFC_KEY | 0x22 | 查找NFC钥匙 |
| CMD_NFC_UNLOCK | 0x23 | NFC解锁 |
| CMD_NFC_LOCK | 0x24 | NFC锁车 |
| CMD_NFC_SWITCH | 0x25 | NFC开关（00关/01开） |
| CMD_OIL_VEHICLE_UNPREVENTION | 0x30 | 油车解防 |
| CMD_OIL_VEHICLE_PREVENTION | 0x31 | 油车防盗 |
| CMD_OIL_VEHICLE_FIND_STATUS | 0x32 | 油车寻车状态查询（带DATA：音量等） |
| CMD_OIL_VEHICLE_UNLOCK_TRUNK | 0x33 | 油车尾箱解锁 |
| CMD_OIL_VEHICLE_LOCK_TRUNK | 0x34 | 油车尾箱锁车 |
| CMD_VEHICLE_UNLOCK_SEAT | 0x35 | 座桶解锁 |
| CMD_VEHICLE_LOCK_SEAT | 0x36 | 座桶锁车 |
| CMD_VEHICLE_MUTE_SETTING | 0x37 | 静音设置（00关/01开） |
| CMD_VEHICLE_UNLOCK_MIDDLE_BOX | 0x38 | 中箱锁解锁 |
| CMD_VEHICLE_LOCK_MIDDLE_BOX | 0x39 | 中箱锁锁车 |
| CMD_VEHICLE_EMERGENCY_MODE_LOCK | 0x40 | 紧急模式打开 |
| CMD_VEHICLE_EMERGENCY_MODE_UNLOCK | 0x41 | 紧急模式关闭 |
| CMD_BLE_MAC_READ | 0x42 | 读取蓝牙MAC地址 |
| CMD_SINGLE_CONTROL_UNLOCK | 0x43 | 单控开锁（尾箱锁） |
| CMD_Assistive_trolley_mode_on | 0x44 | 助力推车模式开启 |
| CMD_Assistive_trolley_mode_off | 0x45 | 助力推车模式关闭 |
| CMD_Assistive_trolley_mode_default | 0x46 | 助力推车默认 |
| CMD_Delayed_headlight_on | 0x47 | 延时大灯开启 |
| CMD_Delayed_headlight_off | 0x48 | 延时大灯关闭 |
| CMD_Delayed_headlight_default | 0x49 | 延时大灯默认 |
| CMD_Delayed_headlight_time_set | 0x50 | 延时大灯时间设置（5~120秒） |
| CMD_Charging_power_set | 0x51 | 充电功率设置（档位：30/22/16/6 kW） |
| CMD_AUTO_P_GEAR_on | 0x52 | 自动P档开启 |
| CMD_AUTO_P_GEAR_off | 0x53 | 自动P档关闭 |
| CMD_AUTO_P_GEAR_default | 0x54 | 自动P档默认 |
| CMD_AUTO_P_GEAR_time_set | 0x55 | 自动P档时间设置（10~120s） |
| CMD_Chord_horn_on | 0x56 | 和弦喇叭开启 |
| CMD_Chord_horn_off | 0x57 | 和弦喇叭关闭 |
| CMD_Chord_horn_default | 0x58 | 和弦喇叭默认 |
| CMD_Chord_horn_type_set | 0x59 | 和弦喇叭音源设置（data 1B） |
| CMD_Chord_horn_volume_set | 0x60 | 和弦喇叭音量设置（data 1B） |
| CMD_Three_shooting_lamp_on | 0x61 | 三射灯氛围灯开启 |
| CMD_Three_shooting_lamp_off | 0x62 | 三射灯氛围灯关闭 |
| CMD_Three_shooting_lamp_default | 0x63 | 三射灯氛围灯默认 |
| CMD_Three_shooting_lamp_mode_set | 0x64 | 三射灯模式设置（效果1B+渐变1B+颜色3B） |
| CMD_Assistive_reversing_gear_on | 0x65 | 辅助倒车档位开启 |
| CMD_Assistive_reversing_gear_off | 0x66 | 辅助倒车档位关闭 |
| CMD_Assistive_reversing_gear_default | 0x67 | 辅助倒车档位默认 |
| CMD_Automatic_steering_reset_on | 0x75 | 自动转向回位开启 |
| CMD_Automatic_steering_reset_off | 0x76 | 自动转向回位关闭 |
| CMD_Automatic_steering_reset_default | 0x77 | 自动转向回位默认 |
| CMD_EBS_switch_on | 0x78 | EBS 开启（data：EBS类型 1B） |
| CMD_Low_speed_gear_on | 0x79 | 低速档位开启 |
| CMD_Low_speed_gear_off | 0x80 | 低速档位关闭 |
| CMD_Low_speed_gear_default | 0x81 | 低速档位默认 |
| CMD_Low_speed_gear_speed_set | 0x82 | 低速档位速度设置（00/01/10/11） |
| CMD_Medium_speed_gear_on | 0x83 | 中速档位开启 |
| CMD_Medium_speed_gear_off | 0x84 | 中速档位关闭 |
| CMD_Medium_speed_gear_default | 0x85 | 中速档位默认 |
| CMD_Medium_speed_gear_speed_set | 0x86 | 中速档位速度设置（00/01/10/11） |
| CMD_High_speed_gear_on | 0x87 | 高速档位开启 |
| CMD_High_speed_gear_off | 0x88 | 高速档位关闭 |
| CMD_High_speed_gear_default | 0x89 | 高速档位默认 |
| CMD_High_speed_gear_speed_set | 0x90 | 高速档位速度设置（00/01/10/11） |
| CMD_Lost_mode_on | 0x91 | 丢失模式开启 |
| CMD_Lost_mode_off | 0x92 | 丢失模式关闭（默认） |
| CMD_TCS_switch_on | 0x93 | TCS 开启 |
| CMD_TCS_switch_off | 0x94 | TCS 关闭 |
| CMD_Side_stand_switch_on | 0x95 | 侧支架开启 |
| CMD_Side_stand_switch_off | 0x96 | 侧支架关闭（注释截断） |
| CMD_Battery_type_set | 0x97 | 电池模式（00铅酸/01锂电） |
| CMD_Battery_capacity_set | 0x98 | 电池容量（单位Ah） |
| CMD_KFA_2KGA_2MQA_data_sync | 0x99 | KFA/2KGA/2MQA 数据同步（车型定义） |
| CMD_HDC_switch_on | 0x100 | HDC 开启（注意：已超过 1B） |
| CMD_HDC_switch_off | 0x101 | HDC 关闭（注意：已超过 1B） |
| CMD_HHC_switch_on | 0x102 | HHC 开启（注意：已超过 1B） |
| CMD_HHC_switch_off | 0x103 | HHC 关闭（注意：已超过 1B） |
| CMD_Starting_strength_set | 0x104 | 起步强度设置（注意：已超过 1B） |
| CMD_SPORT_mode_on | 0x105 | SPORT 开启（注意：已超过 1B） |
| CMD_SPORT_mode_off | 0x106 | SPORT 关闭（注意：已超过 1B） |
| CMD_SPORT_mode_default | 0x107 | SPORT 默认（注意：已超过 1B） |
| CMD_SPORT_mode_type_set | 0x108 | SPORT 类型设置（注意：已超过 1B） |
| CMD_ECO_mode_on | 0x109 | ECO 开启（注意：已超过 1B） |
| CMD_ECO_mode_off | 0x110 | ECO 关闭（注意：已超过 1B） |
| CMD_ECO_mode_default | 0x111 | ECO 默认（注意：已超过 1B） |
| CMD_ECO_mode_type_set | 0x112 | ECO 类型设置（注意：已超过 1B） |
| CMD_Radar_switch_on | 0x113 | 雷达开启（注意：已超过 1B） |
| CMD_Radar_switch_off | 0x114 | 雷达关闭（注意：已超过 1B） |
| CMD_Radar_switch_default | 0x115 | 雷达默认（注意：已超过 1B） |
| CMD_Radar_sensitivity_set | 0x116 | 雷达灵敏度（00低/01中/02高，注意：已超过 1B） |
