# 面部表情系统重设计

**日期**: 2026-05-19
**状态**: 设计完成，待实现

---

## 动机

当前表情系统存在以下局限：

1. **面部结构单一**：整个脸部被建模为一个 `eye_state_t`，只有"眼睛+装饰"，缺乏眉毛、嘴巴、肤色等完整五官
2. **参数粒度不足**：14 个浮点参数控制全局，无法独立操控单个面部组件
3. **动画能力薄弱**：所有参数以同一缓动曲线同步过渡，无法实现层次化的表情切换（如先动眉、再动嘴）
4. **风格不可换**：渲染逻辑硬编码，无法切换不同角色风格

## 目标

将表情系统重构为**组件化、参数化、可换皮**的四层架构：

- 面部拆解为 7 个独立组件（脸型、左右眉、左右眼、嘴、装饰），每个组件通过关键点或属性值参数化
- LVGL 驱动组件参数动画，支持每组件独立时长、延迟、缓动曲线
- 保留扫描线渲染（低内存），支持多套精灵（角色风格）切换
- 两层 API：高层表情预设 + 低层单组件控制

---

## 架构总览

```
┌──────────────────────────────────────────────┐
│  Face API                                    │
│  face_set_expression(HAPPY)     ← 高层       │
│  face_set_component(BROW, ...)  ← 低层       │
├──────────────────────────────────────────────┤
│  Face Animator (LVGL)                        │
│  每组件一个 lv_anim_t，独立时长/延迟/缓动     │
├──────────────────────────────────────────────┤
│  Face Model (纯数据)                          │
│  face_state_t, component_params_t,           │
│  expression_preset_t, sprite_set_t           │
├──────────────────────────────────────────────┤
│  Face Renderer (扫描线, ~3KB RAM)             │
│  逐行分发到各组件 draw 函数, z-order 叠加     │
└──────────────────────────────────────────────┘
```

---

## 第 1 层：Face Model（数据模型）

### 组件类型

```c
typedef enum {
    COMPONENT_FACE = 0,
    COMPONENT_BROW_LEFT,
    COMPONENT_BROW_RIGHT,
    COMPONENT_EYE_LEFT,
    COMPONENT_EYE_RIGHT,
    COMPONENT_MOUTH,
    COMPONENT_DECOR,
    COMPONENT_COUNT
} face_component_t;
```

### 关键点定义

对于需要精确形状控制的组件，使用关键点（每个关键点 = 相对于中性位置的 2D 偏移）：

| 组件 | 关键点 | 属性 |
|------|--------|------|
| 眉毛 (BROW) | `inner` 眉头, `arch` 眉峰, `tail` 眉尾 | `thickness` 粗细 |
| 眼睛 (EYE) | `inner_corner` 内眼角, `outer_corner` 外眼角, `top_lid_mid` 上眼睑中点, `bot_lid_mid` 下眼睑中点, `iris_center` 虹膜中心 | `pupil_scale`, `shine_intensity` |
| 嘴巴 (MOUTH) | `left_corner` 左嘴角, `right_corner` 右嘴角, `upper_lip_mid` 上唇中点, `lower_lip_mid` 下唇中点 | `openness` |

对于简单组件（脸型、装饰），使用纯属性值：

| 组件 | 属性 |
|------|------|
| 脸型 (FACE) | `roundness` (0=尖, 1=圆), `skin_color` |
| 装饰 (DECOR) | `blush`, `tears`, `stars`, `sweat`, `sparkle` (各 0-1) |

关键点坐标均使用归一化坐标系 (-1.0 to 1.0，相对于组件原点)，由精灵的 `base_proportions` 变换为屏幕像素坐标。

### face_state_t — 完整面部状态

所有组件的当前参数值的快照，是动画器的目标、渲染器的输入。

### expression_t — 表情预设

```c
typedef struct {
    const char *name;
    face_state_t target;                                      // 目标状态
    struct {
        uint32_t duration_ms;                                 // 过渡时长
        uint32_t delay_ms;                                    // 延迟启动
        lv_anim_path_cb_t path;                               // 缓动曲线
    } timing[COMPONENT_COUNT];
} expression_t;
```

### sprite_set_t — 精灵套件

```c
typedef struct {
    const char *name;
    // 基础比例（将关键点归一化坐标映射到屏幕像素）
    float eye_size_ratio;
    float eye_spacing_ratio;
    float mouth_y_offset;
    float brow_y_offset;

    // 组件绘制函数表（7 个函数指针）
    void (*draw[COMPONENT_COUNT])(int y, const face_state_t *st,
                                  const sprite_set_t *sp, uint16_t *buf);

    // 默认配色
    color_palette_t palette;
} sprite_set_t;
```

---

## 第 2 层：Face Animator（动画层）

### LVGL 最小化使用

LVGL 仅用于动画计算，不参与渲染。初始化：

```
lv_init()                    // LVGL 内核
lv_tick_inc(1) 每 1ms       // 动画心跳
lv_timer_handler() 每 5ms    // 驱动 lv_anim 更新
```

**不需要** `lv_disp_drv` 和帧缓冲。

### 动画执行策略

表情切换时，对每个组件的当前值和目标值做 diff，有变化的组件创建一个 `lv_anim_t`，驱动 `progress` 从 0→1：

```
face_set_expression(HAPPY):
  快照 current_state → from_state
  遍历组件 diff(from, target):
    有变化 → 创建 lv_anim(progress: 0→1)
              exec_cb: lerp(from, target, progress) → 写入 current_state
  最多 7 个 anim（每组件一个），支持独立 duration / delay / path_cb
```

### 打断处理

动画进行中收到新指令：
- 快照当前插值状态作为新 from_state
- 取消旧的 lv_anim，立即启动新 anim
- 无跳变

### 微动画（低层 API 直接创建 anim）

```
眨眼:  2 个 anim（EYE_L + EYE_R lid_close → lid_open），串联
呼吸:  1 个 anim（FACE roundness），循环
眼球微动: 2 个 anim（EYE_L + EYE_R iris），短周期
```

---

## 第 3 层：Face Renderer（渲染层）

### 渲染循环（扫描线，保留现有方式）

```
for y = 0 → 239:
    fill_background(y, sprite, line_buf)       // 径向渐变肤色
    for comp in render_order:                  // 按 z-order
        sprite->draw[comp](y, st, sprite, buf)
    if eyes_post_line_cb: ...                  // 外部特效钩子
    gc9a01_send_pixels(line_buf, 240)
```

### z-order

```
1. FACE              — 脸型/肤色底图
2. DECOR (腮红子层)   — 眼下方腮红
3. MOUTH             — 嘴巴
4. EYE_LEFT          — 左眼
5. EYE_RIGHT         — 右眼
6. BROW_LEFT         — 左眉（盖眼睛上方）
7. BROW_RIGHT         — 右眉
8. DECOR (叠加子层)   — 眼泪/星星/汗滴（最上层）
```

### 组件绘制函数

每个精灵套件提供 7 个函数，签名统一：

```c
void (*draw)(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf);
```

内部逻辑：
1. 用 sprite 的比例参数将 face_state 的关键点坐标映射到屏幕像素位置
2. 判断扫描行 y 是否在组件边界框内（不在则直接 return）
3. 逐 x 判断像素是否在形状内，在则混合颜色写入 `buf[x]`

### 内存预算

| 项目 | 大小 |
|------|------|
| `line_buf[240]` | 480 B |
| `face_state_t` | ~200 B |
| `sprite_set_t` | ~256 B |
| `expression_t` 表 (13 个) | ~4 KB |
| LVGL 内部 | ~4 KB |
| **合计** | ~9 KB |

远低于全帧缓冲方案（115 KB）。

---

## 第 4 层：Face API

### 高层 API

```c
void face_set_expression(expression_id_t id);   // 表情切换，所有组件动画
void face_set_emotion(emotion_t e);             // 兼容旧接口
```

### 低层 API

```c
void face_set_component(face_component_t comp,
                        const component_params_t *target,
                        uint32_t duration_ms,
                        uint32_t delay_ms,
                        lv_anim_path_cb_t path);

void face_set_component_instant(face_component_t comp,
                                const component_params_t *target);

const face_state_t *face_get_current_state(void);
```

### 精灵管理

```c
void face_set_sprite(sprite_id_t id);           // 切换精灵（保持状态）
```

切换精灵只改变绘制函数表和配色，`face_state` 不变。如需重置表情，调用方自行调用 `face_set_expression(NEUTRAL)`。

### 调用关系

```
app_behavior    ──高层──→ face_set_expression(HAPPY)
app_display     ──低层──→ face_set_component(BROW_LEFT, ...)
app_effects     ──钩子──→ eyes_post_line_cb（不变）
```

---

## 默认精灵（迁移现有代码）

现有 `expressive_eyes.c` 的渲染逻辑将成为**默认精灵 "classic"** 的绘制实现：

- `draw_eye()` ← 现有 `render_eye()`，扩展为完整眼睑关键点
- `draw_brow()` ← 新增（当前眉毛逻辑混合在 `render_eye` 的 `curve_up/down` 参数中）
- `draw_mouth()` ← 新增
- `draw_face()` ← 现有背景渐变逻辑，增强为肤色底图
- `draw_decor()` ← 现有 `render_blush/tears/stars`

后续新增精灵（如 "cat"、"pixel"）提供另一套绘制函数，共享相同的参数接口。

---

## 与现有模块的集成

### 新增文件

```
components/face_system/
├── face_model.h/c          — 数据模型定义 + 表情预设表
├── face_animator.h/c       — LVGL 动画驱动
├── face_renderer.h/c       — 扫描线渲染调度
├── face_api.h/c            — 两层 API
├── sprites/
│   ├── sprite_classic.c    — 默认精灵（迁移现有逻辑）
│   └── sprite_*.c          — 未来精灵
└── CMakeLists.txt
```

### 修改文件

| 文件 | 变更 |
|------|------|
| `main/app_display.c` | 改为使用 `face_api.h`，微动画走低层 API |
| `main/app_behavior.c` | 保持不变（仍然调用高层表情接口） |
| `main/main.c` | 添加 LVGL 初始化 + `lv_timer_handler` 调用 |
| `components/expressive_eyes/` | 保留但标记 deprecated，后续清理 |

### FreeRTOS 任务变更

| 任务 | 变更 |
|------|------|
| `display_task` | 追加 `lv_timer_handler()` 调用（每 5ms），然后 `face_renderer_render_frame()` |
| 其他任务 | 不变 |

---

## 迁移策略

### Phase 1: 基础框架 + 默认精灵
1. 创建 `components/face_system/`，实现四层架构
2. 默认精灵 "classic" 迁移现有 `expressive_eyes` 渲染逻辑
3. 保持 `app_behavior → app_display → face_api` 调用链不变
4. 13 个预设表情一一映射到新 `expression_t`
5. 验证：表情切换效果与迁移前一致

### Phase 2: 增强
1. 增强默认精灵的眉毛和嘴巴绘制（当前不具备）
2. 添加新精灵（cat、pixel 等）
3. 利用低层 API 增强微动画层次

### Phase 3: 清理
1. 移除 `expressive_eyes` 组件
2. 移除废弃代码

---

## 开放问题

- 眉毛和嘴巴的默认精灵绘制算法需在实现阶段细化（当前无参考实现）
- LVGL 的 `lv_timer_handler` 调用频率对整体帧率的影响需实测
- 精灵文件组织方式：编译期链接 vs 运行时注册（建议编译期，代码量小）
