# Props System Design

## Summary

给表情系统添加小型"道具组件"（爱心、茶杯、小手、星星、汗滴），悬浮在脸周围，
扩展 `decor_params_t`，复用 decor_overlay 渲染流程，由行为层通过 face_api 调用触发。

## Decision Log

| Decision | Choice | Rationale |
|----------|--------|-----------|
| 空间关系 | 悬浮/环绕 | 漫画贴纸效果，不附着脸上 |
| 触发方式 | 表情 + 传感器 | 表情预设可带 props，传感器事件也能触发 |
| 架构方案 | 扩展 decor 层 | 改动最小，复用 decor_overlay 绘制流程 |

---

## Data Model

### 新增类型（`face_model.h`）

```c
typedef enum {
    PROP_NONE = 0,
    PROP_HEART,        // 爱心
    PROP_TEACUP,       // 茶杯
    PROP_HAND,         // 小手
    PROP_STAR_SMALL,   // 小星星
    PROP_SWEAT_DROP,   // 汗滴
    PROP_COUNT
} prop_type_t;

typedef struct {
    prop_type_t type;
    float angle;       // 极坐标角度 (0=右, π/2=上)，弧度
    float distance;    // 距脸中心距离 (0.0~1.0, 1.0 = 100px, 距屏幕边缘留余量)
    float scale;       // 0.0 ~ 1.0
    float opacity;     // 0.0 ~ 1.0（淡入/淡出用）
} prop_instance_t;
```

### 扩展 decor_params_t

```c
typedef struct {
    // 原有字段
    float blush;
    float tears;
    float stars;
    float sweat;
    float sparkle;
    // 新增
    uint8_t prop_count;
    prop_instance_t props[3];
} decor_params_t;
```

最多同时激活 3 个 prop。参数放在 `decor_params_t` 中，跟随 `face_state_t` 传递，
无需修改渲染器接口（sprite 的 `draw_decor_overlay` 签名不变）。

---

## 动画系统适配

### 问题

现有 animator 的 `decor_params_lerp` 对所有字段做 float 线性插值。
prop 数组不能简单 float lerp——切换表情时不应清除或变形当前 props。

### 方案

1. **所有 expression preset 的 `decor` 目标设 `prop_count = 0`**。
   Props 不由表情驱动，切换表情时 props 保持不变。

2. **`decor_params_lerp` 扩展**：float 字段正常 lerp；prop 数组从 current state 原样拷贝到 out。
   效果：表情过渡期间，props 不受 lerp 影响，始终保留当前值。

```c
static void decor_params_lerp(const decor_params_t *a, const decor_params_t *b,
                              float t, decor_params_t *out) {
    // float 字段正常 lerp
    out->blush   = a->blush   + (b->blush   - a->blush)   * t;
    out->tears   = a->tears   + (b->tears   - a->tears)   * t;
    out->stars   = a->stars   + (b->stars   - a->stars)   * t;
    out->sweat   = a->sweat   + (b->sweat   - a->sweat)   * t;
    out->sparkle = a->sparkle + (b->sparkle - a->sparkle) * t;
    // prop 数组原样拷贝（不做 lerp）
    out->prop_count = a->prop_count;
    memcpy(out->props, a->props, sizeof(a->props));
}
```

3. **Prop 动画独立管理**：新增 `face_prop.c`，在 `face_render_frame()` 中
   `micro_animator_apply()` 之后调用 `prop_animator_apply()`，驱动 prop 的
   opacity/scale/angle 过渡。

---

## Prop 动画器（`face_prop.c` / `face_prop.h`）

每帧遍历 `decor.props[]`，对每个激活的 prop 执行：

- **淡入/淡出**：修改 `opacity`（ease-in/ease-out，默认 200ms）
- **悬浮 bobbing**：小幅度正弦波动，周期 ~1.8s
- **自动清理**：opacity = 0 保持 2 帧后，移除该 prop（移位填补）

```c
void prop_animator_init(void);
void prop_animator_apply(face_state_t *s);  // 每帧调用
void prop_show(face_state_t *s, prop_type_t type, float angle, float distance, uint32_t duration_ms);
void prop_hide(face_state_t *s, prop_type_t type, uint32_t duration_ms);
void prop_clear(face_state_t *s, uint32_t duration_ms);
```

---

## 渲染

### face_api.h 集成点调整

```c
static inline void face_init(void) {
    animator_init();
    renderer_init();
    micro_animator_init();
    prop_animator_init();           // NEW
    renderer_set_sprite(sprite_registry_default());
}

static inline void face_render_frame(void) {
    face_state_t display_state = *animator_get_state();
    micro_animator_apply(&display_state);
    prop_animator_apply(&display_state);  // NEW
    renderer_render_frame(&display_state);
}
```

### render_frame 调整（`face_renderer.c`）

在第 8 遍（`draw_decor_overlay`）之后新增第 9 遍：

```c
void renderer_render_frame(const face_state_t *st) {
    // ... 现有 8 遍不变 ...
    sp->draw_decor_overlay(y, st, sp, line_buf);

    // 第 9 遍：Props
    sp->draw_props(y, st, sp, line_buf);  // NEW
}
```

### sprite_set_t 新增函数指针（`face_model.h`）

```c
typedef struct sprite_set_s {
    // ... 现有字段不变 ...
    sprite_draw_func_t draw_decor_overlay;
    sprite_draw_func_t draw_props;    // NEW
    // ...
} sprite_set_t;
```

### Sprite 实现指南

每个 sprite 需实现 `draw_props`。基本流程：

```c
void sprite_xxx_draw_props(int y, const face_state_t *st,
                            const sprite_set_t *sp, uint16_t *buf) {
    for (int i = 0; i < st->decor.prop_count; i++) {
        const prop_instance_t *p = &st->decor.props[i];
        if (p->opacity <= 0.01f) continue;

        // 极坐标 → 屏幕坐标
        float cx = 120.0f, cy = 120.0f;  // 屏幕中心
        float r = 100.0f * p->distance;   // 半径
        float px = cx + r * cosf(p->angle);
        float py = cy - r * sinf(p->angle);

        uint16_t color = /* prop 颜色，混合背景色用 p->opacity */;

        switch (p->type) {
        case PROP_HEART:   draw_heart(y, px, py, p->scale, color, buf); break;
        case PROP_TEACUP:  draw_teacup(y, px, py, p->scale, color, buf); break;
        case PROP_HAND:    draw_hand(y, px, py, p->scale, color, buf); break;
        case PROP_STAR_SMALL: draw_star(y, px, py, p->scale, color, buf); break;
        case PROP_SWEAT_DROP: draw_sweat(y, px, py, p->scale, color, buf); break;
        default: break;
        }
    }
}
```

每个 prop 的绘制使用 `face_common.h` 已有的 scanline 基元（fill_circle、draw_ring 等），
不引入新的曲线类型。坐标、scale 换算到像素后在 sprite 内部处理，sprite 独立决定 prop
的视觉风格。

---

## face_api.h 新增接口

`face_api.h` 包含 `face_prop.h` 并提供 thin wrapper。实际实现在 `face_prop.c` 中，
通过 `animator_get_state()` 访问当前状态（`face_prop.c` include `face_animator.h`）。

```c
// face_prop.h 声明（实现在 face_prop.c）
void face_prop_show(prop_type_t type, float angle, float distance, uint32_t duration_ms);
void face_prop_hide(prop_type_t type, uint32_t duration_ms);
void face_prop_clear(uint32_t duration_ms);

// face_api.h 中直接 expose（无需 wrapper，因为 face_prop.h 已包含）
```

---

## 首批 Prop 绘制规格

| Prop | 形状 | 大概尺寸(1x scale) | 颜色来源 | 典型触发场景 |
|------|------|-------------------|---------|-------------|
| PROP_HEART | 心形 (两个圆弧+底部尖角) | ~18px | palette 中的 accent 色 | HEART_EYES, 碰一碰挚友 |
| PROP_TEACUP | 椭圆杯身 + 弧形把手 + 三条蒸汽线 | ~22px | palette warm 色 | WARM 表情, 捂热事件 |
| PROP_HAND | 简化手掌 (圆角矩形 + 拇指) | ~20px | palette skin 色 | 摸头回应, 挥手 |
| PROP_STAR_SMALL | 五角星 (fill_circle + 尖角) | ~14px | palette gold | EXCITED 表情, 摇晃事件 |
| PROP_SWEAT_DROP | 水滴形 (椭圆 + 顶部尖) | ~10px | palette blue | 紧张, 害羞 |

---

## 受影响文件清单

| 文件 | 变更 |
|------|------|
| `face_model.h` | 新增 prop_type_t, prop_instance_t；decor_params_t 扩展；sprite_set_t 新增 draw_props |
| `face_model.c` | FACE_STATE_NEUTRAL 和所有 EXPRESSION_DEFS 的 decor 字段补 prop_count=0 |
| `face_common.h` | 新增 prop 绘制辅助函数（draw_heart_scan 等）或放在各 sprite 内 |
| `face_animator.c` | decor_params_lerp 扩展 |
| `face_prop.h` (NEW) | prop 动画器头文件 |
| `face_prop.c` (NEW) | prop 动画器实现：show/hide/clear + 每帧 apply |
| `face_renderer.c` | 新增第 9 遍 draw_props 调用 |
| `face_api.h` | 新增 face_prop_show/hide/clear；init/render_frame 调整 |
| `sprites/sprite_*.c` (6 个) | 每个实现 draw_props |
| `main/app_behavior.c` | 调用 face_prop_show 触发 props |

---

## 不在范围内

- Prop 碰撞/重叠处理（简单按数组顺序绘制，后绘覆盖先绘）
- Prop 由 LVGL animator 驱动（使用独立小状态机，与 micro_animator 风格一致）
- 超过 3 个同时 prop（硬限制 3，超出时忽略新请求）
- Prop 样式随 sprite 大幅变化（统一形状，颜色由 palette 控制即可区分风格）
