# Phase 2: 精灵增强与新角色 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 增强 classic 精灵渲染质量 + 新增 3 套角色精灵 + 微动画层次提升

**Architecture:** 扩展 face_palette 和 face_model，每个精灵独立渲染文件，微动画在 app_display.c 增强

**Tech Stack:** ESP32-S3, FreeRTOS, C, scanline rendering

---

### Task 1: face_palette.h — 新增颜色和调色板

**Files:**
- Modify: `components/face_system/face_palette.h`

- [ ] **Step 1: 新增 PAL_TONGUE 索引和 3 套新调色板**

在 `palette_index_t` 中添加 `PAL_TONGUE`，定义 `PALETTE_CAT`、`PALETTE_PIXEL`、`PALETTE_ROBOT` 静态数组。

```c
// 在 palette_index_t 枚举中新增:
PAL_TONGUE,      // 舌头粉色

// 新增静态调色板:
static const uint16_t PALETTE_CAT[PAL_COUNT] = {
    [PAL_BG]       = RGB565(30, 28, 36),   // 深灰背景
    [PAL_BG_EDGE]  = RGB565(18, 16, 24),   // 更深边缘
    [PAL_IRIS]     = RGB565(255, 200, 60), // 金色虹膜
    [PAL_PUPIL]    = RGB565(20, 18, 20),   // 暗瞳
    [PAL_SHINE]    = RGB565(255, 245, 200),// 暖高光
    [PAL_BLUSH]    = RGB565(255, 140, 140),// 粉色腮红
    [PAL_TEAR]     = RGB565(160, 210, 255),// 水蓝眼泪
    [PAL_STAR]     = RGB565(255, 230, 130),// 暖星
    [PAL_SKIN]     = RGB565(60, 55, 70),   // 暗肤
    [PAL_BROW]     = RGB565(100, 95, 90),  // 深灰眉
    [PAL_MOUTH]    = RGB565(200, 160, 160),// 粉唇
    [PAL_TONGUE]   = RGB565(240, 120, 120),// 粉舌
};

static const uint16_t PALETTE_PIXEL[PAL_COUNT] = {
    [PAL_BG]       = RGB565(255, 255, 255),// 纯白背景
    [PAL_BG_EDGE]  = RGB565(200, 200, 200),// 浅灰边缘
    [PAL_IRIS]     = RGB565(0, 180, 60),   // 亮绿虹膜
    [PAL_PUPIL]    = RGB565(0, 0, 0),      // 纯黑瞳
    [PAL_SHINE]    = RGB565(255, 255, 255),// 白高光
    [PAL_BLUSH]    = RGB565(255, 100, 100),// 红腮红(像素格)
    [PAL_TEAR]     = RGB565(100, 180, 255),// 蓝泪
    [PAL_STAR]     = RGB565(255, 255, 0),   // 亮黄星
    [PAL_SKIN]     = RGB565(255, 220, 180),// 肤色
    [PAL_BROW]     = RGB565(80, 50, 30),   // 棕眉
    [PAL_MOUTH]    = RGB565(60, 30, 20),   // 深唇线
    [PAL_TONGUE]   = RGB565(255, 80, 100), // 红舌
};

static const uint16_t PALETTE_ROBOT[PAL_COUNT] = {
    [PAL_BG]       = RGB565(40, 42, 48),   // 金属灰背景
    [PAL_BG_EDGE]  = RGB565(25, 27, 32),   // 暗面板
    [PAL_IRIS]     = RGB565(60, 200, 255), // 蓝LED眼
    [PAL_PUPIL]    = RGB565(10, 30, 60),   // 深蓝暗瞳
    [PAL_SHINE]    = RGB565(180, 230, 255),// 蓝白发光
    [PAL_BLUSH]    = RGB565(255, 80, 60),  // 红LED腮
    [PAL_TEAR]     = RGB565(100, 150, 255),// 蓝液滴
    [PAL_STAR]     = RGB565(255, 220, 60), // 金指示灯
    [PAL_SKIN]     = RGB565(70, 75, 85),   // 金属面板
    [PAL_BROW]     = RGB565(50, 55, 60),   // 面板缝
    [PAL_MOUTH]    = RGB565(180, 190, 200),// 银色网格
    [PAL_TONGUE]   = RGB565(255, 60, 50),  // 红指示灯
};
```

- [ ] **Step 2: Commit**

---

### Task 2: sprite_classic.c — A1 眉毛锥度

**Files:**
- Modify: `components/face_system/face_model.h`
- Modify: `components/face_system/face_model.c`
- Modify: `components/face_system/sprite_classic.c`

- [ ] **Step 1: face_model.h 新增 brow taper 参数**

```c
typedef struct {
    face_kpt_t inner;
    face_kpt_t arch;
    face_kpt_t tail;
    float thickness;   // 基础粗细 (arch 处)
    float taper;       // 锥度 0=无, 1=最大
} brow_params_t;
```

更新 BROW_NEUTRAL 默认 `.taper = 0.6f`

- [ ] **Step 2: draw_brow_impl 支持锥度**

修改 `draw_brow_impl`：沿贝塞尔曲线动态计算线宽，arch 处 = thickness*4，尾端 = thickness*4*(1-taper*0.6)，眉头 = thickness*4*(1-taper*0.3)

```c
// 在 draw_brow_impl 中, 计算当前 t 处的线宽:
float width_t = bp->thickness * 4.0f;
float inner_mult = 1.0f - bp->taper * 0.3f;  // 眉头 70% of arch
float tail_mult = 1.0f - bp->taper * 0.6f;   // 眉尾 40% of arch
float width_mult = inner_mult + (tail_mult - inner_mult) * t;
float half_thick = width_t * width_mult;
```

- [ ] **Step 3: Commit**

---

### Task 3: sprite_classic.c — A2 嘴巴增强

**Files:**
- Modify: `components/face_system/face_palette.h` (PAL_TONGUE — done in Task 1)
- Modify: `components/face_system/sprite_classic.c`

- [ ] **Step 1: draw_mouth 添加舌头**

```c
// 在 draw_mouth 的开口区域中:
// 当 openness > 0.2 且 y 在口腔下半部时:
if (mp->openness > 0.2f && y > (upper_y + lower_y) * 0.5f) {
    // 椭圆舌头
    float tongue_cx = CENTER_X;
    float tongue_cy = (upper_y + lower_y) * 0.5f + openness_offset * 0.3f;
    float tongue_rx = half_width * 0.4f;
    float tongue_ry = openness_offset * 0.35f;
    float tongue_dist_sq = (fx*tongue_ry)*(fx*tongue_ry) + (fy_t*tongue_rx)*(fy_t*tongue_rx);
    if (tongue_dist_sq < tongue_rx * tongue_rx * tongue_ry * tongue_ry) {
        buf[x] = blend_colors(buf[x], pal[PAL_TONGUE], 0.9f);
        continue;
    }
}
```

- [ ] **Step 2: 不对称嘴角支持**

`left_corner.dy` 和 `right_corner.dy` 影响嘴角高度，已部分支持。确保传入的 dy 值影响 lcx_dy / rcx_dy。

- [ ] **Step 3: Commit**

---

### Task 4: sprite_classic.c — A3 眼角关键点 + A4 脸型 roundness

**Files:**
- Modify: `components/face_system/sprite_classic.c`

- [ ] **Step 1: eye corner keypoints 集成到眼睑公式**

```c
// 在 draw_eye_impl 的眼睑计算中:
float inner_adj = ep->inner_corner.dy * 5.0f;  // 内眼角偏移
float outer_adj = ep->outer_corner.dy * 5.0f;  // 外眼角偏移
// 沿 x 轴线性插值:
float corner_adj = inner_adj + (outer_adj - inner_adj) * ((fx + eye_r) / (2 * eye_r));
float top_lid = base_top + (5.0f * lid_open * arc) + corner_adj * 0.5f;
float bot_lid = base_bot - (3.0f * lid_open * arc) - corner_adj * 0.3f;
```

- [ ] **Step 2: face roundness 调制背景**

```c
// 在 build_bg_lut 或 draw_face 中:
float r = st->face.roundness;
// 混合曼哈顿距离和欧几里得距离
float manhattan = fabsf(dx) + fabsf(dy);
float euclidean = sqrtf(dx * dx + dy * dy);
float mixed_dist = euclidean + (1.0f - r * 2.0f) * (manhattan - euclidean) * 0.5f;
if (mixed_dist < 0) mixed_dist = 0;
int d = (int)mixed_dist;
```

- [ ] **Step 3: Commit**

---

### Task 5: sprite_cat.h/c — 猫精灵

**Files:**
- Create: `components/face_system/sprites/sprite_cat.h`
- Create: `components/face_system/sprites/sprite_cat.c`

- [ ] **Step 1: sprite_cat.h**

```c
#ifndef SPRITE_CAT_H
#define SPRITE_CAT_H
#include "face_model.h"
extern const sprite_set_t SPRITE_CAT;
#endif
```

- [ ] **Step 2: sprite_cat.c — 猫耳朵 + 竖瞳 + 猫嘴 + 胡须**

实现:
- `draw_cat_face`: 深色背景（复用 draw_face）
- `draw_cat_ear_impl`: 三角形耳朵（内耳粉色）
- `draw_cat_eye_impl`: 竖瞳（菱形瞳孔，pupil_scale 控制宽度）
- `draw_cat_mouth`: "ω" 形（三点曲线，中间点上扬）
- `draw_cat_whiskers`: 左右各 3 根胡须曲线
- `draw_cat_blush`: 复用 draw_blush
- `draw_cat_decor`: 复用 draw_decor_overlay

竖瞳关键代码:
```c
// 菱形瞳孔: 替换圆形 pupil 判定
float dx_abs = fabsf(fx - iris_cx);
float dy_abs = fabsf(fy - iris_cy);
float slit_width = pupil_r * (1.0f - ep->pupil_scale); // pupil_scale=1→圆, 0→细缝
float pupil_dist = dx_abs / pupil_r + dy_abs / (pupil_r * 3.0f);
if (pupil_dist < 1.0f - slit_width * 0.8f) {
    buf[x] = pal[PAL_PUPIL];
}
```

猫嘴 "ω" 形:
```c
// 使用两个二次贝塞尔形成 ω 形状
// 左半: 嘴角→上唇弯→中间上翘点
// 右半: 中间上翘点→上唇弯→嘴角
float omega_y = mouth_cy - 6.0f; // 中间上翘
float t_left = (x - lcx) / (CENTER_X - lcx + 0.001f);
float t_right = (x - CENTER_X) / (rcx - CENTER_X + 0.001f);
float upper_y;
if (x < CENTER_X) {
    upper_y = (1-t_left)*(1-t_left)*lc_dy + 2*(1-t_left)*t_left*omega_y + t_left*t_left*mouth_cy;
} else {
    upper_y = (1-t_right)*(1-t_right)*mouth_cy + 2*(1-t_right)*t_right*omega_y + t_right*t_right*rc_dy;
}
```

- [ ] **Step 3: Commit**

---

### Task 6: sprite_pixel.h/c — 像素精灵

**Files:**
- Create: `components/face_system/sprites/sprite_pixel.h`
- Create: `components/face_system/sprites/sprite_pixel.c`

- [ ] **Step 1: sprite_pixel.h**

```c
#ifndef SPRITE_PIXEL_H
#define SPRITE_PIXEL_H
#include "face_model.h"
extern const sprite_set_t SPRITE_PIXEL;
#endif
```

- [ ] **Step 2: sprite_pixel.c — 块状像素渲染**

所有坐标使用 `(int)floor` 量化到 3-4px 网格。无抗锯齿。

```c
// 像素眼: 所有坐标量化
int grid = 4; // 4px grid
int px = (x / grid) * grid + grid/2;
int py = (y / grid) * grid + grid/2;
// 用块状判定替代连续距离
if (abs(px - eye_cx) < eye_r && abs(py - eye_cy) < eye_r) {
    buf[x] = pal[PAL_IRIS]; // 纯色, 无渐变
}
```

像素瞳孔: 2×2 grid 黑块
像素虹膜: 纯色无渐变
像素高光: 1 grid 白块
嘴巴: 2px 宽水平线

- [ ] **Step 3: Commit**

---

### Task 7: sprite_robot.h/c — 机械精灵

**Files:**
- Create: `components/face_system/sprites/sprite_robot.h`
- Create: `components/face_system/sprites/sprite_robot.c`

- [ ] **Step 1: sprite_robot.h**

```c
#ifndef SPRITE_ROBOT_H
#define SPRITE_ROBOT_H
#include "face_model.h"
extern const sprite_set_t SPRITE_ROBOT;
#endif
```

- [ ] **Step 2: sprite_robot.c — 六边形眼 + LED 发光 + 面板线**

六边形眼眶:
```c
// 六边形判定: 6 条半平面
// 简化: 用矩形+切角
int eye_w = 28, eye_h = 22;
float dx_abs = fabsf(fx);
float dy_abs = fabsf(fy);
float hex_dist = fmaxf(dx_abs / eye_w + dy_abs / eye_h, fmaxf(dx_abs / eye_w, dy_abs / eye_h));
if (hex_dist < 1.0f) { /* inside hex */ }
```

LED 发光效果:
```c
// 中心发光 + 径向衰减 + 光晕
float glow = expf(-r_sq / (iris_r * iris_r * 0.3f));
buf[x] = blend_colors(buf[x], pal[PAL_SHINE], glow * 0.7f);
```

面板线 decor:
```c
// 垂直线 x = CENTER_X - 40, CENTER_X + 40
// 水平线 y = CENTER_Y - 30
// 细线 1-2px 宽, 暗色
```

- [ ] **Step 3: Commit**

---

### Task 8: face_model.c + CMakeLists — 注册新精灵

**Files:**
- Modify: `components/face_system/face_model.c` (extern declarations)
- Modify: `components/face_system/CMakeLists.txt` (add new C files)

- [ ] **Step 1: face_model.c 添加 extern 声明**

```c
extern const sprite_set_t SPRITE_CAT;
extern const sprite_set_t SPRITE_PIXEL;
extern const sprite_set_t SPRITE_ROBOT;
```

- [ ] **Step 2: CMakeLists.txt 添加源文件**

```cmake
idf_component_register(
    SRCS "face_model.c" "face_animator.c" "face_renderer.c" "sprite_classic.c"
         "sprites/sprite_cat.c" "sprites/sprite_pixel.c" "sprites/sprite_robot.c"
    INCLUDE_DIRS "." "sprites"
    REQUIRES lvgl gc9a01
)
```

- [ ] **Step 3: Commit**

---

### Task 9: app_display.c — 微动画增强

**Files:**
- Modify: `main/app_display.c`

- [ ] **Step 1: 不对称眨眼 (Wink)**

```c
static int wink_type = 0; // 0=both, 1=left only, 2=right only
// 在 blink_state 从 0→1 转换时:
if (blink_state == 1 && blink_t == 0) {
    wink_type = (esp_random() % 5 == 0) ? (1 + (esp_random() % 2)) : 0;
}
// 眨眼应用时:
float left_lid_target = (wink_type == 2) ? 0 : lid_close;
float right_lid_target = (wink_type == 1) ? 0 : lid_close;
```

- [ ] **Step 2: 眉毛微动**

```c
static float brow_micro_phase_left = 0;
static float brow_micro_phase_right = 0;
brow_micro_phase_left += 0.05f + (esp_random() % 100) * 0.0001f;
brow_micro_phase_right += 0.07f + (esp_random() % 100) * 0.0001f;
float brow_micro = sinf(brow_micro_phase_left) * 0.015f;
// 仅在无动画过渡时应用（检查 animator 状态）
brow_params_t bp_l = st->brow[0];
bp_l.arch.dy += brow_micro;
face_set_component_instant(COMPONENT_BROW_LEFT, &bp_l);
```

- [ ] **Step 3: 空闲嘴部微动**

```c
static int frames_until_mouth_move = 180;
static int mouth_move_state = 0;
// mouth_move_state: 0=静, 1=微张, 2=闭合
// 用 face_set_component_instant 微调 openness
```

- [ ] **Step 4: saccade 随机化**

```c
// 将固定幅值改为随机:
static float saccade_amplitude = 0.05f;
static int saccade_timer = 0;
saccade_timer--;
if (saccade_timer <= 0) {
    saccade_amplitude = 0.02f + (esp_random() % 70) * 0.001f;
    saccade_timer = 90 + (esp_random() % 180);
}
float micro_x = sinf(sac_phase * 1.3f) * saccade_amplitude;
// 偶发性大跳动:
if (esp_random() % 300 == 0) {
    micro_x += (esp_random() % 100 - 50) * 0.002f;
}
```

- [ ] **Step 5: Commit**

---

### Task 10: Build verification

- [ ] **Step 1: Configure + Build**

```bash
idf.py reconfigure && idf.py build
```

- [ ] **Step 2: Fix any compilation errors**

- [ ] **Step 3: Commit fixes**

---

## Plan Self-Review

### Spec Coverage
| Spec Section | Task(s) |
|---|---|
| A1. 眉毛锥度 | T2 |
| A2. 嘴巴增强 | T1, T3 |
| A3. 眼角关键点 | T4 |
| A4. 脸型 roundness | T4 |
| B1. Cat 精灵 | T1, T5 |
| B2. Pixel 精灵 | T1, T6 |
| B3. Robot 精灵 | T1, T7 |
| C1. 不对称眨眼 | T9 |
| C2. 眉毛微动 | T9 |
| C3. 空闲嘴微动 | T9 |
| C4. saccade 随机化 | T9 |
| 注册新精灵 | T8 |
| 构建验证 | T10 |
