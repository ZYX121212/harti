# 面部表情交互优化 v2 — 设计规格

**日期**: 2026-05-21
**方案**: B — 动画系统升级 + 渲染打磨
**目标**: 让面部表情更好看、更自然，涵盖渲染细节、动画表现力、表情性格、交互反馈四个维度

---

## 1. 架构概览

```
behavior ──> blend_t { expr_A, expr_B, weight }    (新增：双表情混合)
     │
     v
animator v2                                          (重写：三相位 + 混合)
     │  ANTICIPATE → ATTACK → SETTLE
     │  squash/stretch 动态注入
     v
face_state (扩展参数)
     │
     v
app_display v2                                       (重写：noise 引擎)
     │  hash noise 替代 sin()
     │  Pareto 分布替代均匀随机
     │  微动衰减协调
     v
renderer (不变)                                      (8 pass 扫描线合成)
     │
     v
sprite_classic v2                                    (增强：虹膜/嘴型/睫毛)
```

**不改动的模块**: `face_renderer.c/h`（扫描线合成器保持不变）

---

## 2. face_model 参数扩展

### 2.1 face_params_t

```c
typedef struct {
    float roundness;    // 现有
    float squash_x;     // 新增：水平挤压 (-1.0~1.0)
    float stretch_y;    // 新增：垂直拉伸 (-1.0~1.0)
} face_params_t;
```

`squash_x` / `stretch_y` 为瞬态参数，所有表情预设中为 0，由 animator 在过渡期间动态注入。

### 2.2 eye_params_t

```c
typedef struct {
    // ... 现有字段不变 ...
    float iris_detail;  // 新增：虹膜环纹强度 (0.0~1.0)，控制角膜缘环+第三高光
    float eyelash;      // 新增：睫毛可见度 (0.0~1.0)
} eye_params_t;
```

### 2.3 mouth_params_t

```c
typedef struct {
    // ... 现有字段不变 ...
    float cupid_depth;  // 新增：唇峰深度 (0.0~1.0)
    float tooth_show;   // 新增：露齿程度 (0.0~1.0)
} mouth_params_t;
```

### 2.4 expression_blend_t（新结构体）

```c
typedef struct {
    expression_id_t expr_a;   // 主表情
    expression_id_t expr_b;   // 副表情
    float blend;              // 0.0 = pure A, 1.0 = pure B
} expression_blend_t;
```

行为层不使用此结构体，仍然调用 `face_set_expression(id)`。blend 为 animator 内部用于过渡的实现细节。

### 2.5 预设默认值策略

新增参数在 NEUTRAL 状态下均为 0/默认值。各表情预设中只在有意义的场景设置非零值：

| 参数 | 非零表情 |
|------|----------|
| squash_x / stretch_y | 瞬态，animator 注入 |
| iris_detail | NEUTRAL=0.5, EXCITED/SURPRISED=0.8, SLEEPY=0.2 |
| eyelash | NEUTRAL=0.6, CONTENT/SLEEPY=0.8, SURPRISED=0.3 |
| cupid_depth | NEUTRAL=0.4, HAPPY=0.7, SAD=0.1 |
| tooth_show | HAPPY=0.6, EXCITED=0.9, 其他=0 |

---

## 3. Animator v2 — 三相位过渡 + 混合

### 3.1 相位状态机

每个 component 独立追踪四相位：

```
IDLE → ANTICIPATE (~60ms) → ATTACK (主体时长) → SETTLE (~100ms) → IDLE
```

- **ANTICIPATE**: 向目标相反方向微移，幅度 = 主位移的 15-25%，ease-out
- **ATTACK**: 使用组件配置的 path_type 向目标过渡，可含 overshoot
- **SETTLE**: ease-in-out 回到精确目标，释放 squash/stretch

### 3.2 双表达式混合

animator 内部追踪 `from_expr` 和 `to_expr`，通过 `blend_t` (0→1) 插值：

```
blended_state = lerp_params(from_expr.target, to_expr.target, effective_t)
```

`effective_t` 根据当前相位计算：
- ANTICIPATE: `t = -anticipation_amt * ease_out(phase_t)`（先反向）
- ATTACK: `t = -anticipation_amt + (1.0 + anticipation_amt) * path_func(phase_t)`（前进到目标）
- SETTLE: `t = 1.0 + overshoot * (1.0 - ease_in_out(phase_t))`（回稳）

### 3.3 挤压拉伸

仅在快速过渡时激活（ATTACK 阶段 duration < 200ms）：

```
squash_x = amplitude * sin(blend_t * PI) * active_flag
stretch_y = amplitude * sin(blend_t * PI + PI/2) * active_flag
amplitude = 0.15
```

慢速过渡（≥300ms）不触发，避免拖沓感。

### 3.4 公共 API（与当前一致）

```c
void animator_set_expression(expression_id_t id);
void animator_set_component(face_component_t comp, const void *target,
                            uint32_t dur, uint32_t delay, anim_path_type_t path);
const face_state_t *animator_get_state(void);
void animator_tick(void);
```

---

## 4. app_display v2 — Noise 引擎 + 微动画重构

### 4.1 Hash-based Value Noise

```c
// 基于整数 hash 的 value noise
static uint32_t hash_uint(uint32_t x); // FNV 变体

static float noise1d(uint32_t tick, float stride) {
    float x = tick * stride;
    int i = (int)x;
    float t = x - i;
    t = t * t * (3.0f - 2.0f * t); // smoothstep
    float v0 = (float)(hash_uint(i) & 0xFFFF) / 32768.0f - 1.0f;
    float v1 = (float)(hash_uint(i + 1) & 0xFFFF) / 32768.0f - 1.0f;
    return v0 + (v1 - v0) * t;
}

// 多倍频简化（直接读不同 stride，不做完整 fBm）
static float micro_noise(uint32_t tick, float base_stride) {
    return noise1d(tick, base_stride) * 0.7f
         + noise1d(tick, base_stride * 3.7f) * 0.2f
         + noise1d(tick, base_stride * 7.1f) * 0.1f;
}
```

每个 component 使用独立的 phase（base tick + offset）和 stride。

### 4.2 微动画改造

| 动画 | 当前 | 改为 |
|------|------|------|
| 呼吸 | `sin(fixed_phase) * 0.05` | `micro_noise(tick, 0.003) * 0.06` |
| 眨眼间隔 | 均匀随机 120-360 帧 | Pareto 分布：大部分 3-5s，偶尔 1s 双眨 |
| 眨眼速度 | 固定 0.40/0.25 | noise 微调 ±20% |
| 扫视 | `sin(phase) * amp` | noise 漂移 + 随机大幅跳变 (Pareto 幅度) |
| 眉毛微动 | `sin(L/R_phase) * 0.022` | 左右独立 noise，偶尔单侧挑眉 (1/200) |
| 嘴部微动 | 固定周期开合 | noise 不规则微张 + Pareto 静默时长 |
| 倾斜反馈 | 直接映射 g_tilt | 加低通滤波消除传感器抖动 |

### 4.3 微动衰减协调

animator 过渡期间，微动幅度按抛物线衰减：

```
micro_scale = 1.0 - 4.0 * (blend_t - 0.5)^2
// blend_t=0.0 → 1.0 (全开)
// blend_t=0.5 → 0.0 (最小)
// blend_t=1.0 → 1.0 (全开)
micro_scale = clamp(micro_scale, 0.1, 1.0)
```

---

## 5. Sprite 渲染增强

### 5.1 虹膜三层渐变 + 角膜缘环

在 `draw_eye_impl()` 内部，iris 区域内新增：
- **角膜缘环**：iris 最外圈 4px 暗色环（`blend_colors(iris, pupil, 0.8)`）
- **中间层**：limbal ring 内侧到 pupil 外侧之间，从 iris 渐变到 iris_dark
- **第三高光**：极小 (r=2.0) 低透明度高光点，`iris_detail` 控制可见度

额外像素计算：每个 iris 像素多 1 次 if + 可选 blend。

### 5.2 唇峰（Cupid's Bow）

上唇从单段贝塞尔 → 两段贝塞尔：

```
// 当前:
upper_y = (1-t)^2 * corner_y + 2*(1-t)*t * uly + t^2 * corner_y

// 改为:
mid_x = (lcx + rcx) / 2
cupid_y = uly + cupid_depth * 8.0  // cupid's bow 下凹
// 左半: lcx → mid_x, 控制点取左唇峰
// 右半: mid_x → rcx, 控制点取右唇峰
```

零额外像素操作，仅曲线计算变化。

### 5.3 牙齿

当 `tooth_show > 0 && openness > 0.15` 时，在张嘴区域的上半部分绘制白色齿带：
- 宽度 = 嘴宽的 60%
- 高度 = openness * 8px
- 使用 `PAL_SCLERA` 颜色（白色），上下边缘 blend 融合

### 5.4 睫毛

沿上眼睑曲线分布 7-9 根暗色短线（`PAL_PUPIL`）：
- 中间最长（~8px），两侧递减
- `eyelash` 参数控制透明度
- 仅在上眼睑边缘 2px 范围内绘制

### 5.5 鼻梁阴影

两眼之间、面部中心位置，一个极淡的垂直渐变阴影：
- 宽度 ~16px，高度 ~35px
- 使用 `PAL_BG_EDGE`，透明度 0.10-0.15
- 不可配置，始终渲染

---

## 6. 性能预算

| 改动 | 额外开销 | 缓解措施 |
|------|----------|----------|
| 虹膜 3 层 | +1 blend/iris_pixel | 仅 iris 区域（占全屏 ~15%） |
| 睫毛 | ~20 像素/眼/行 | 仅上眼睑受影响 |
| 唇峰 | 0 额外像素 | 仅曲线计算 |
| 牙齿 | ~200 像素/张嘴帧 | 仅在张嘴时触发 |
| 鼻梁阴影 | ~600 像素/帧 | 极少量 blend |
| noise 引擎 | hash + smoothstep | 整数运算为主，无浮点表 |
| 双表达式混合 | 2x lerp per component | component 数量固定 (7) |
| 三相位状态机 | 微小状态追踪 | 每帧少量 if/else |

**预期帧率影响**: <10%，维持 ≥20fps。

---

## 7. 测试策略

### 7.1 单元级
- noise 引擎：统计分布验证（Pareto 间隔分布、输出范围 [-1,1]）
- animator 相位：验证 ANTICIPATE→ATTACK→SETTLE 时序正确，无相位跳跃
- 参数混合：验证 blend_t=0/0.5/1.0 时的插值精度

### 7.2 集成级
- 所有 13 个表情切换无崩溃、无 NaN
- 快速连续切换表情（压力测试）
- 双主题（白/黑）渲染一致

### 7.3 视觉验收
- 在 240x240 GC9A01 实机上目视检查各表情
- 确认睫毛不超出眼睑边界
- 确认牙齿在张嘴表情中可见且位置正确
- 确认唇峰在 HAPPY/SAD/NEUTRAL 中有视觉差异
