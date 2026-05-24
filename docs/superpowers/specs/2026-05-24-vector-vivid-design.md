# Vector Vivid: 表情交互深度优化

## Summary

通过两个新模块（`face_vivid` + `face_temperament`）为 vector sprite 添加四个维度的交互改进：道具动态动画、表情微动态、过渡表演感、性格染色。不改动现有 `face_model` / `face_animator` 数据结构，采用模块级叠加方式。

## Architecture

```
face_render_frame() 调用顺序：
  1. animator_get_state()        → 拿到当前 lerp 后的 face_state_t
  2. micro_animator_apply()      → 眨眼、视线、呼吸、倾斜 (现有)
  3. prop_animator_apply()       → 道具出现/消失 (现有)
  4. face_vivid_apply()          → NEW: 道具动态 + 五官微变
  5. face_temperament_apply()    → NEW: 性格染色 + 过渡脉冲
  6. renderer_render_frame()     → 绘制输出
```

两个模块放在 `components/face_system/` 下。

---

## Module 1: face_vivid (道具动态 + 五官微变)

### 文件

- 新增: `components/face_system/face_vivid.h`
- 新增: `components/face_system/face_vivid.c`

### 接口

```c
void face_vivid_init(void);
void face_vivid_apply(face_state_t *state);
```

### Part B — 道具动态

每帧修改 `decor_params_t.props[]` 中活跃道具的 `angle`/`distance`/`scale` 参数，制造动画：

| 道具 | 动画 | 机制 |
|------|------|------|
| TEACUP_STEAM | 杯子轻微上下浮动 + 脉动 | `distance` ±0.03 sin (1.2Hz) + `scale` ±0.05 sin (1.2Hz) |
| MUSIC_NOTE | 左右摇摆 + 弹跳 | `angle` ±0.05 sin (2.5Hz) + `scale` ±0.08 sin(5.0Hz, 轻快感) |
| SUNGLASSES | 滑入淡入 + 微晃 | 出现时 `opacity` 0→1 (150ms); `distance` 0.02 sin (0.6Hz) |
| HEART | 脉动缩放 | `scale` ±0.06 sin (1.5Hz) |
| STAR_SMALL | 旋转 + 微闪烁 | `angle` 持续漂移 0.3rad/s + `scale` ±0.04 sin (3Hz) |
| TEACUP | 轻微浮动 | `distance` ±0.02 sin (1.0Hz) |

各道具使用不同频率，避免同步。

### Part C — 五官微动态

在 expression 基准参数上叠加微小正弦扰动：

| 部位 | 字段 | 幅度 | 频率 | 备注 |
|------|------|------|------|------|
| 眼睛位置 | eye.position.dx/dy | ±0.02 | 0.7Hz | 双眼反相 |
| 眉毛拱高 | brow.arch.dy | ±0.015 | 1.3Hz | 30% 时间门控生效 |
| 嘴角 | mouth.left_corner.dx, right_corner.dx | ±0.01 | 0.4Hz | 与 breathing 错开 90° |
| 瞳孔缩放 | eye.pupil_scale | ±0.03 | 1.8Hz | 模拟心跳频率 |

全局相位 `micro_phase` 每帧递增 `frame_delta_ms`。

---

## Module 2: face_temperament (过渡表演感 + 性格染色)

### 文件

- 新增: `components/face_system/face_temperament.h`
- 新增: `components/face_system/face_temperament.c`

### 接口

```c
typedef struct {
    float energy;          // 0.0..1.0  表达强度
    float responsiveness;  // 0.0..1.0  反应速度
    float expressiveness;  // 0.0..1.0  夸张度
    float quirk;           // 0.0..1.0  意外表情概率
} temperament_profile_t;

extern const temperament_profile_t TEMPERAMENT_DEFAULT;   // {0.6, 0.5, 0.5, 0.1}
extern const temperament_profile_t TEMPERAMENT_CHILL;     // {0.3, 0.3, 0.3, 0.2}
extern const temperament_profile_t TEMPERAMENT_DRAMATIC;  // {0.9, 0.8, 0.9, 0.3}

void face_temperament_init(void);
void face_temperament_set_profile(const temperament_profile_t *p);
void face_temperament_notify_expression_change(expression_id_t old_id, expression_id_t new_id);
void face_temperament_apply(face_state_t *state);
```

### Part A — 过渡脉冲

每次表情切换时，`face_temperament_notify_expression_change()` 被调用。根据目标表情在关键参数上加短暂偏移（300ms 线性衰减）：

| 表情 | 脉冲参数 | 偏移量 |
|------|---------|--------|
| HAPPY | mouth.corner.dx + brow.arch.dy | corner ±0.05, arch -0.03 |
| SURPRISED | eye.top_lid_mid.dy + mouth.openness | lid -0.04, openness +0.05 |
| SAD | eye.top_lid_mid.dy + mouth.corner.dy | lid +0.03, corner +0.02 |
| COLD | eye.pupil_scale | -0.05 |
| EXCITED | mouth.openness + eye.shine_intensity | openness +0.08, shine +0.15 |
| ANGRY | brow.thickness + mouth.corner.dy | thickness +0.2, corner +0.03 |
| WARM | eye.shine_intensity + blush | shine +0.1, blush +0.15 |
| 其他 | 无脉冲 | — |

脉冲在 `face_temperament_apply()` 中每帧累加衰减，300ms 后归零。

### Part D — 性格染色

`temperament_profile_t` 四个参数影响：

| 参数 | 影响目标 | 机制 |
|------|---------|------|
| `energy` | 表情参数幅度 | 目标值 × (1.0 + (energy - 0.5) × 0.2)，即能量高放大 10%、低缩小 10% |
| `responsiveness` | 过渡速度 | 调用 `animator_set_component()` 覆盖 timing，乘系数 0.7~1.5 |
| `expressiveness` | 夸张度 | brow arch 额外多下沉 (exp - 0.5)×0.05; mouth corner 额外多拉开 (exp - 0.5)×0.03 |
| `quirk` | 意外表情 | 每次切换有 quirk×100% 概率触发一个附加动作（随机 blink / 快速单眉挑 / 快速 wink） |

每次传感器事件触发表情时，在 profile 基准上加 ±0.08 随机偏移，确保「每次不一样但气质一致」。

### 性格切换

- 开机默认 `TEMPERAMENT_DEFAULT`
- 日后可通过 BLE 指令切换：`SET_TEMPERAMENT CHILL` / `SET_TEMPERAMENT DRAMATIC`
- 当前不实现 BLE 指令，预留 `face_temperament_set_profile()` API 即可

---

## Integration Points

### face_api.h 改动

```c
// 在 face_init() 中加入:
vivid_init();
temperament_init();

// 新增 API（可选，供 BLE 日后使用）:
static inline void face_set_temperament(const temperament_profile_t *p) {
    face_temperament_set_profile(p);
}
```

### face_render_frame() 改动

在 `prop_animator_apply(&display_state)` 之后加入：

```c
vivid_apply(&display_state);
temperament_apply(&display_state);
```

### main/app_behavior.c 改动

在 `transition_to()` 中加入通知调用：

```c
static expression_id_t prev_expression = EMOTION_NEUTRAL;
static void transition_to(behavior_state_t state, emotion_t emo) {
    // ... existing code ...
    face_temperament_notify_expression_change(prev_expression, (expression_id_t)emo);
    prev_expression = (expression_id_t)emo;
    face_set_expression((expression_id_t)emo);
    // ...
}
```

### tempo 调用 animator

`face_temperament.c` 需要 `#include "face_animator.h"` 以调用 `animator_set_component()` 覆盖 timing。这是唯一跨模块调用。如果 animator 不支持外部调整 timing，则 `responsiveness` 参数改为通过缩放 animator tick 速率实现。

---

## Files Summary

| File | Action | Description |
|------|--------|-------------|
| `components/face_system/face_vivid.h` | Create | 道具动态 + 五官微变 API |
| `components/face_system/face_vivid.c` | Create | 实现 (~120 行) |
| `components/face_system/face_temperament.h` | Create | 性格染色 + 过渡脉冲 API |
| `components/face_system/face_temperament.c` | Create | 实现 (~150 行) |
| `components/face_system/face_api.h` | Modify | 加入 vivid_init / temperament_init / face_set_temperament |
| `main/app_behavior.c` | Modify | transition_to 中 notify expression change |

## Out of Scope

- BLE 命令切换性格（预留 API 但不实现协议）
- 其他 8 种 sprite（只做 vector）
- face_model / face_animator 数据结构改动
- 性格随时间的自动漂移
