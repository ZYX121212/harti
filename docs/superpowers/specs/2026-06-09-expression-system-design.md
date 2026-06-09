# 表情系统优化设计文档

**日期**：2026-06-09  
**范围**：新增 2 个表情、重设交互映射、新增 3 段微动画  
**不改动**：眼睛大小、睫毛、腮红、调色板、其他 sprite 风格

---

## 一、新增表情

### EMOTION_DIZZY（ID = 14）

**触发**：摇晃事件（EVT_SHAKE）

**视觉**：
- 眼型：`V_EYE_DIZZY` — 保留圆形白色轮廓，瞳孔替换为五角星（白色填充）
- 嘴型：`V_MOUTH_DIZZY` — O 形小圆口（椭圆轮廓，无填充）
- 装饰：decor 层绘制 3 颗小星星 `✦`，分布在脸部左上、右上、右侧，固定位置（不受 decor.sparkle 控制，由表情参数直接驱动）

**参数（`EXPRESSION_DEFS[14]`）**：
- `eye.pupil_scale = 0.0f`（与 HEART_EYES 共享此值，靠 iris_detail 区分）
- `eye.iris_detail = 1.0f`（DIZZY 专用标志位；HEART_EYES 是 0.6f，所有现有表情 ≤ 0.8f）
- `classify_eye` 中 DIZZY 分支须排在 HEART_EYES 之前：`pupil_scale < 0.05f && iris_detail > 0.95f → V_EYE_DIZZY`
- `eye.top_lid_mid.dy = 0.0f`，`eye.bot_lid_mid.dy = 0.0f`（眼睛完全睁开）
- `mouth.openness = 0.15f`，`mouth.cupid_depth = 0.2f`（触发 O 形嘴）
- `brow.arch.dy = 0.1f`（眉毛略下压，茫然感）
- `decor.sparkle = 1.0f`（标志位，renderer 据此绘制星星）
- 过渡时长：所有 component 80ms，PATH_EASE_OUT（快速进入眩晕）

**生命周期**：
- 进入 DIZZY 后，behavior 启动 2000ms 单次定时器
- 定时器到期后自动 transition_to(STATE_IDLE, EMOTION_NEUTRAL)
- 若在 2000ms 内再次触发 EVT_SHAKE，重置定时器（不重复进入表情）

---

### EMOTION_UPSIDE_DOWN（ID = 15）

**触发**：翻转事件（EVT_FLIP，即设备 Z 轴朝下持续 >500ms）

**视觉**：
- 眼型：`V_EYE_UPSIDE_DOWN` — 正常圆形轮廓，下眼睑区域填充半透明白色弧（模拟泪水积聚，opacity ≈ 40%），瞳孔正常大小但位置偏下
- 眉毛：内高外低委屈弧，通过 `brow.inner.dy = -0.2f`、`brow.tail.dy = 0.3f` 实现
- 嘴型：走现有 `V_SAD` 分支但嘴角下垂更明显（`mouth.left_corner.dy = 0.22f`）
- 装饰：两侧各一颗泪珠，通过 `decor.tears = 0.8f` 触发现有泪珠绘制逻辑

**参数（`EXPRESSION_DEFS[15]`）**：
- `eye.pupil_scale = 0.45f`
- `eye.iris_center.dy = 0.2f`（瞳孔下沉）
- `eye.iris_detail = 0.9f`（UPSIDE_DOWN 标志位；现有最高 0.8f，安全唯一）
- `eye.shine_intensity = 0.6f`
- `brow.inner.dy = -0.2f`，`brow.tail.dy = 0.3f`，`brow.arch.dy = 0.05f`
- `mouth.left_corner.dy = 0.22f`，`mouth.right_corner.dy = 0.22f`
- `mouth.openness = 0.05f`，`mouth.cupid_depth = 0.1f`
- `decor.tears = 0.8f`
- 过渡时长：150ms，PATH_EASE_IN_OUT
- `classify_eye` 中 UPSIDE_DOWN 分支：在 SAD / BORED / HAPPY 之后，NORMAL 之前加 `iris_detail > 0.85f → V_EYE_UPSIDE_DOWN`（此时 top_lid_mid.dy=0，不会被 SLEEPY/BORED 截获）

**生命周期**：
- 翻转后持续停留在 UPSIDE_DOWN
- 设备 Z 轴回正（az > +0.7，持续 >300ms）触发 `EVT_FLIP_RESTORE`（新增传感器事件）
- 收到 EVT_FLIP_RESTORE → transition_to(STATE_IDLE, EMOTION_NEUTRAL)
- 超时保护：UPSIDE_DOWN 持续 >30s 无回正 → 自动恢复 NEUTRAL（防止永久卡住）

**classify_eye 新增分支**（优先级在 V_EYE_HEART 之后）：
```c
/* ⑨ UPSIDE_DOWN: 泪眼，iris_center.dy > 0.15 且 tears > 0.5 */
if (ep->iris_center.dy > 0.15f && /* decor.tears > 0.5 需传入 decor */ ...) return V_EYE_UPSIDE_DOWN;
```
> 注：`classify_eye` 当前只接收 `eye_params_t` + `brow_params_t`，需新增 `const decor_params_t *dp` 参数，或改为用 `iris_detail = 0.9f` 作为 UPSIDE_DOWN 的内部标志位（推荐后者，避免改函数签名）。

---

## 二、交互映射重设（`app_behavior.c`）

### tap 计数分三档

现有 `EVT_TAP` 的 `msg.value` 已经是连续 tap 次数。修改 `on_event` 中 `EVT_TAP` 分支：

```
tap_count == 1  → HAPPY（被摸头，轻柔正向）
tap_count == 2  → SURPRISED（被戳，快速惊跳）
tap_count >= 3  → SAD（被烦了，委屈）
```

### 事件映射全表

| 事件 | 条件 | 新目标表情 | 附加行为 |
|------|------|-----------|---------|
| EVT_TAP | value == 1 | HAPPY | — |
| EVT_TAP | value == 2 | SURPRISED | — |
| EVT_TAP | value >= 3 | SAD | — |
| EVT_SHAKE | 任意 | DIZZY | 2s 后自动恢复；重复摇晃重置计时器 |
| EVT_FLIP | Z朝下 | UPSIDE_DOWN | 等待 EVT_FLIP_RESTORE 或 30s 超时 |
| EVT_FLIP_RESTORE | Z回正 | NEUTRAL | 仅在 STATE_UPSIDE_DOWN 时响应 |
| EVT_TWIST | 任意 | EXCITED | 修正（原为 SURPRISED） |
| EVT_TILT | dir 0/1/2/3 | CONFUSED | 保留现有逻辑 |
| EVT_WARM_UP | — | WARM | + face_prop_show(PROP_HEART, ...) |
| EVT_COLD_DOWN | — | COLD | + 颤抖微动画激活 |
| EVT_BLE_MEET | — | SURPRISED | 占位（BLE 未实现） |
| EVT_BLE_FRIEND | — | HAPPY | + PROP_HEART（已有） |

### 新 behavior_state

```c
STATE_DIZZY,
STATE_UPSIDE_DOWN,
```

### DIZZY 状态机

```
enter DIZZY:
  transition_to(STATE_DIZZY, EMOTION_DIZZY)
  start timer 2000ms

on EVT_SHAKE while STATE_DIZZY:
  reset timer to 2000ms（不重新播放表情）

timer expires:
  transition_to(STATE_IDLE, EMOTION_NEUTRAL)
```

### UPSIDE_DOWN 状态机

```
enter UPSIDE_DOWN:
  transition_to(STATE_UPSIDE_DOWN, EMOTION_UPSIDE_DOWN)

on EVT_FLIP_RESTORE:
  if current_state == STATE_UPSIDE_DOWN:
    transition_to(STATE_IDLE, EMOTION_NEUTRAL)

timeout 30s:
  transition_to(STATE_IDLE, EMOTION_NEUTRAL)
```

---

## 三、新增微动画（`face_micro.c`）

微动画通过 `micro_animator_apply()` 内部检测当前表情状态。需要 `face_micro` 能访问当前表情 ID，方式：新增 `micro_animator_set_expression(expression_id_t id)` 接口，在 `face_api.h` 的 `face_set_expression()` 中同步调用。

### 3-A COLD 颤抖

**激活条件**：当前表情 ID == EMOTION_COLD  
**实现**：
- 在 `face_micro.c` 中用 `lv_tick_get()` 计时，以 8Hz（周期 125ms）正弦波叠加 `squash_x`
- 振幅：`±0.025f`
- 公式：`s->face.squash_x += sinf(now_ms * 2π * 8 / 1000) * 0.025f`
- 同步叠加 `stretch_y` 反相（保持面积守恒感）：`s->face.stretch_y -= sinf(...) * 0.015f`

### 3-B BORED 叹气

**激活条件**：当前表情 ID == EMOTION_BORED，且距上次叹气 > rand(8000, 15000) ms  
**动画序列**（状态机，在 micro 层独立维护）：

```
SIGH_IDLE → (随机触发) → SIGH_CLOSING（400ms，上眼睑缓降到 +0.45）
          → SIGH_HOLD（200ms，保持闭眼）
          → SIGH_OPENING（600ms，缓慢睁开）
          → SIGH_IDLE，重置随机计时器
```

- 闭眼通过叠加 `top_lid_mid.dy += sigh_t * 0.45f` 实现（不覆盖表情基础值）

### 3-C SLEEPY 惊醒

**激活条件**：当前表情 ID == EMOTION_SLEEPY，且距上次惊醒 > rand(12000, 20000) ms  
**动画序列**：

```
STARTLE_IDLE → (随机触发) → STARTLE_OPEN（120ms，眼睑快速恢复，pupil_scale 回 0.6）
             → STARTLE_HOLD（500ms，定格清醒状）
             → STARTLE_CLOSE（300ms，再次闭上）
             → STARTLE_IDLE，重置计时器
```

---

## 四、EVT_FLIP_RESTORE 新传感器事件

在 `app_sensors.h` 中新增：
```c
EVT_FLIP_RESTORE,  // Z轴从负回正（设备翻回正面朝上）
```

在 `app_sensors.c` 的 `process_imu()` 中，在现有 flip 检测之后添加回正检测：
- 条件：`az > +0.7f` 持续 `>= 30 frames`（300ms）且 `flip_armed == false`（即之前触发过 flip）
- 触发后发送 `EVT_FLIP_RESTORE`，将 `flip_armed` 重置为 `true`

---

## 五、文件改动清单

| 文件 | 改动 |
|------|------|
| `face_model.h` | 添加 `EMOTION_DIZZY = 14`、`EMOTION_UPSIDE_DOWN = 15` |
| `face_model.c` | 添加对应 `EXPRESSION_DEFS[14]`、`[15]` 参数与 timing |
| `sprite_vector.c` | 添加 `V_EYE_DIZZY`、`V_EYE_UPSIDE_DOWN` 眼型；添加 `V_MOUTH_DIZZY` 嘴型；decor 层绘制 DIZZY 星星；修改 `classify_eye` 分支顺序 |
| `face_micro.h` | 添加 `micro_animator_set_expression(expression_id_t id)` |
| `face_micro.c` | 实现 COLD 颤抖、BORED 叹气、SLEEPY 惊醒三段状态机 |
| `face_api.h` | `face_set_expression()` 中同步调用 `micro_animator_set_expression()` |
| `app_sensors.h` | 添加 `EVT_FLIP_RESTORE` |
| `app_sensors.c` | 添加翻转回正检测逻辑 |
| `app_behavior.c` | 重写 `on_event()`：tap 三档、shake→DIZZY、flip→UPSIDE_DOWN、twist→EXCITED；添加 STATE_DIZZY/STATE_UPSIDE_DOWN；新增 DIZZY 计时器和 UPSIDE_DOWN 超时 |

**不改动**：`face_palette.c`、`face_common.h`、其他 sprite_*.c、`gc9a01.c`、`face_renderer.c`

---

## 六、实现约束

- 严格黑白配色：DIZZY 星星和 UPSIDE_DOWN 泪水均使用 `pal[PAL_SCLERA]`（白色）
- DIZZY 的 `iris_detail = 1.0f` 作为内部标志位，不暴露给其他模块
- UPSIDE_DOWN 的 `iris_detail = 0.9f` 作为内部标志位（与 DIZZY 区分）
- 所有新微动画均为纯叠加偏移，不修改 `current_state`（与现有设计一致）
- `face_micro.c` 内部维护一个 `current_expr_id` 变量，仅由 `micro_animator_set_expression()` 写入
