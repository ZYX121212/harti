# Vector Eye Types (2→8) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 vector sprite 眼型从 2 种扩展到 8 种，覆盖全部 13 种情绪

**Architecture:** 重写 `classify_eye()` 阈值分类逻辑（新增 brow_params_t 参数），在 `draw_eye_vector()` 中新增 6 个眼型绘制分支，调整 `face_model.c` 中部分表情的 eye 参数

**Tech Stack:** C (ESP-IDF), 无测试框架（编译即验证）

---

## 文件结构

| 文件 | 职责 |
|------|------|
| `components/face_system/sprites/sprite_vector.c` | eye_type_t 枚举、classify_eye 分类、draw_eye_vector 绘制 |
| `components/face_system/face_model.c` | 表情预设的 eye 参数微调 |

---

### Task 1: 扩展 eye_type_t 枚举 + 重写 classify_eye

**Files:**
- Modify: `components/face_system/sprites/sprite_vector.c:19-47`

- [ ] **Step 1: 替换 eye_type_t 枚举**

将现有的 `V_EYE_NORMAL, V_EYE_SLEEPY` 替换为 8 种：

```c
typedef enum {
    V_EYE_NORMAL,      // ① 圆眼
    V_EYE_HAPPY,       // ② 弯月眼
    V_EYE_SURPRISED,   // ③ 大圆眼
    V_EYE_SAD,         // ④ 垂眼
    V_EYE_HEART,       // ⑤ 心形瞳
    V_EYE_ANGRY,       // ⑥ 锐角眼
    V_EYE_BORED,       // ⑦ 半垂眼
    V_EYE_SLEEPY,      // ⑧ 横线眼
} eye_type_t;
```

- [ ] **Step 2: 重写 classify_eye 函数签名和实现**

将 `classify_eye(const eye_params_t *ep)` 改为接受 brow_params_t：

```c
static eye_type_t classify_eye(const eye_params_t *ep, const brow_params_t *bp) {
    /* ⑧ SLEEPY: top lid heavily drooped */
    if (ep->top_lid_mid.dy > 0.22f) return V_EYE_SLEEPY;

    /* ⑤ HEART: pupil scaled to near-zero → heart pupil */
    if (ep->pupil_scale < 0.05f) return V_EYE_HEART;

    /* ⑥ ANGRY: thick brows + inner brows pulled down */
    if (bp->thickness > 1.2f && bp->inner.dy < -0.12f) return V_EYE_ANGRY;

    /* ③ SURPRISED: wide-open (top lid up, small pupil) */
    if (ep->top_lid_mid.dy < -0.10f && ep->pupil_scale < 0.48f) return V_EYE_SURPRISED;

    /* ④ SAD: pupil sinks downward */
    if (ep->iris_center.dy > 0.25f) return V_EYE_SAD;

    /* ⑦ BORED: half-lidded (top lid droops but not fully sleepy) */
    if (ep->top_lid_mid.dy > 0.12f) return V_EYE_BORED;

    /* ② HAPPY: bottom lid pushed up (squint) + top lid slightly down */
    if (ep->bot_lid_mid.dy > 0.08f && ep->top_lid_mid.dy < -0.04f) return V_EYE_HAPPY;

    /* ① NORMAL: default round eye */
    return V_EYE_NORMAL;
}
```

- [ ] **Step 3: 编译验证**

```bash
cd /Users/nova/proj/harti && . ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

- [ ] **Step 4: Commit**

```bash
git add components/face_system/sprites/sprite_vector.c
git commit -m "feat: expand eye_type_t to 8 types and rewrite classify_eye

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: 实现 6 个新眼型的 draw_eye_vector 分支

**Files:**
- Modify: `components/face_system/sprites/sprite_vector.c:58-179`

- [ ] **Step 1: 重构 draw_eye_vector 为 switch 结构**

保留现有 V_EYE_NORMAL（lines 95-178）和 V_EYE_SLEEPY（lines 73-93）代码，将其包装进 switch 分支：

```c
static void draw_eye_vector(int y, const eye_params_t *ep, float eye_r,
                            int eye_cx, int eye_cy, eye_type_t etype,
                            const uint16_t *pal, uint16_t *buf) {
    float fy = y - eye_cy;
    float pad = 2.5f;
    if (fy < -eye_r - pad || fy > eye_r + pad) return;

    int x_start = eye_cx - (int)eye_r - (int)pad;
    int x_end   = eye_cx + (int)eye_r + (int)pad;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    switch (etype) {
    case V_EYE_SLEEPY:
        // 现有 V_EYE_SLEEPY 代码 (lines 73-93)
        break;
    case V_EYE_HAPPY:
        // 弯月眼（新增）
        break;
    case V_EYE_SURPRISED:
        // 大圆眼（新增）
        break;
    case V_EYE_SAD:
        // 垂眼（新增）
        break;
    case V_EYE_HEART:
        // 心形瞳（新增）
        break;
    case V_EYE_ANGRY:
        // 锐角眼（新增）
        break;
    case V_EYE_BORED:
        // 半垂眼（新增）
        break;
    case V_EYE_NORMAL:
    default:
        // 现有 NORMAL 代码 (lines 95-178)
        break;
    }
}
```

- [ ] **Step 2: 实现 V_EYE_HAPPY（弯月眼）**

在 V_EYE_SLEEPY case 之后添加：

```c
    case V_EYE_HAPPY: {
        /* Crescent eye: downward-curving arc, no pupil */
        float arc_depth = 6.0f + (1.0f - ep->bot_lid_mid.dy) * 8.0f;
        float half_w = eye_r * 0.85f;
        float control_y = eye_cy + arc_depth;
        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            if (fabsf(fx) > half_w) continue;
            float t = (fx + half_w) / (2.0f * half_w);
            float curve_y = (1.0f - t) * (1.0f - t) * eye_cy
                          + 2.0f * (1.0f - t) * t * (eye_cy + control_y)
                          + t * t * eye_cy;
            if (fabsf(y - curve_y) < 2.0f) {
                buf[x] = pal[PAL_SCLERA];
            }
        }
        break;
    }
```

- [ ] **Step 3: 实现 V_EYE_SURPRISED（大圆眼）**

```c
    case V_EYE_SURPRISED: {
        /* Wide-open eye: enlarged ring, small pupil, thin outline */
        float r_ext = eye_r * 1.12f;
        float eye_r_sq = r_ext * r_ext;
        float pupil_dx = ep->iris_center.dx * r_ext * 0.5f;
        float pupil_dy = ep->iris_center.dy * r_ext * 0.5f;
        float pupil_r = eye_r * 0.22f * (0.5f + ep->pupil_scale * 0.5f);
        if (pupil_r < 2.5f) pupil_r = 2.5f;
        float pupil_r_sq = pupil_r * pupil_r;

        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            float r_sq = fx * fx + fy * fy;
            if (r_sq >= eye_r_sq) continue;
            float r = sqrtf(r_sq);
            /* thin outline (~1.5px) */
            if (r_ext - r < 1.5f) {
                buf[x] = pal[PAL_SCLERA];
                continue;
            }
            /* small pupil */
            float p_dx = fx - pupil_dx;
            float p_dy = fy - pupil_dy;
            if (p_dx * p_dx + p_dy * p_dy < pupil_r_sq) {
                buf[x] = pal[PAL_SCLERA];
            }
        }
        break;
    }
```

- [ ] **Step 4: 实现 V_EYE_SAD（垂眼）**

```c
    case V_EYE_SAD: {
        /* Droopy eye: vertical ellipse, pupil sunk low */
        float ry = eye_r * 0.82f;
        float ry_sq = ry * ry;
        float pupil_dx = ep->iris_center.dx * eye_r * 0.5f;
        float pupil_dy = ep->iris_center.dy * eye_r * 0.6f;
        float pupil_r = eye_r * 0.28f * (0.4f + ep->pupil_scale * 0.6f);
        if (pupil_r < 2.5f) pupil_r = 2.5f;
        float pupil_r_sq = pupil_r * pupil_r;

        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            /* ellipse check: (fx/rx)^2 + (fy/ry)^2 < 1 */
            float e = (fx * fx) / (eye_r * eye_r) + (fy * fy) / ry_sq;
            if (e >= 1.0f) continue;
            /* outline */
            if (sqrtf(e) > 0.88f) {
                buf[x] = pal[PAL_SCLERA];
                continue;
            }
            /* pupil */
            float p_dx = fx - pupil_dx;
            float p_dy = fy - pupil_dy;
            if (p_dx * p_dx + p_dy * p_dy < pupil_r_sq) {
                buf[x] = pal[PAL_SCLERA];
            }
        }
        break;
    }
```

- [ ] **Step 5: 实现 V_EYE_HEART（心形瞳）**

```c
    case V_EYE_HEART: {
        /* Heart pupil: normal ring + heart shape in pupil area */
        float eye_r_sq = eye_r * eye_r;
        float pupil_cx = eye_cx + ep->iris_center.dx * eye_r * 0.4f;
        float pupil_cy = eye_cy + ep->iris_center.dy * eye_r * 0.4f;
        float h_scale = eye_r * 0.38f * (0.5f + ep->iris_detail * 0.5f);

        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            float r_sq = fx * fx + fy * fy;
            if (r_sq >= eye_r_sq) continue;
            float r = sqrtf(r_sq);
            /* white outline ring */
            if (eye_r - r < 2.2f) {
                buf[x] = pal[PAL_SCLERA];
                continue;
            }
            /* heart shape in pupil area — already uses PAL_PUPIL (black) */
            /* Check if point is inside heart centered at (pupil_cx, pupil_cy) */
            float hx = (x - pupil_cx) / h_scale;
            float hy = (y - pupil_cy) / h_scale;
            /* heart implicit: (x^2 + y^2 - 1)^3 - x^2*y^3 < 0 */
            float h2 = hx * hx + hy * hy;
            if (h2 < 2.5f) {
                float hv = (h2 - 1.0f) * (h2 - 1.0f) * (h2 - 1.0f) - hx * hx * hy * hy * hy;
                if (hv < 0.0f) {
                    buf[x] = pal[PAL_PUPIL];  // black heart fill
                }
            }
        }
        break;
    }
```

- [ ] **Step 6: 实现 V_EYE_ANGRY（锐角眼）**

```c
    case V_EYE_ANGRY: {
        /* Angular eye: inverted-V upper lid + small pupil centered */
        float inner_x = eye_cx - eye_r * 0.9f;
        float outer_x = eye_cx + eye_r * 0.9f;
        float mid_y_up = eye_cy - eye_r * 0.6f;
        float mid_y_lo = eye_cy - eye_r * 0.25f;
        float pupil_r = eye_r * 0.25f * (0.5f + ep->pupil_scale * 0.5f);
        if (pupil_r < 2.0f) pupil_r = 2.0f;
        float pupil_r_sq = pupil_r * pupil_r;
        float pupil_dx = ep->iris_center.dx * eye_r * 0.4f;
        float pupil_dy = ep->iris_center.dy * eye_r * 0.3f + eye_r * 0.05f;

        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            /* Check if inside the angular eye shape */
            /* Upper lid: line from inner_x to mid_y_up to outer_x */
            float t = (float)(x - inner_x) / (outer_x - inner_x);
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            float lid_y = (1.0f - t) * (1.0f - t) * eye_cy
                        + 2.0f * (1.0f - t) * t * mid_y_up
                        + t * t * eye_cy;
            /* Lower lid: flat line at about eye_cy + r*0.5 */
            float bot_y = eye_cy + eye_r * 0.4f;
            if (y < lid_y || y > bot_y) continue;
            /* Draw white outline on lid perimeter */
            if (fabsf(y - lid_y) < 1.8f || fabsf(y - bot_y) < 1.5f) {
                if (fabsf(fx) < eye_r * 0.95f) {
                    buf[x] = pal[PAL_SCLERA];
                }
            }
            /* Pupil */
            float p_dx = fx - pupil_dx;
            float p_dy = fy - pupil_dy;
            if (p_dx * p_dx + p_dy * p_dy < pupil_r_sq) {
                buf[x] = pal[PAL_SCLERA];
            }
        }
        break;
    }
```

- [ ] **Step 7: 实现 V_EYE_BORED（半垂眼）**

```c
    case V_EYE_BORED: {
        /* Half-lidded: flat arc upper lid, pupil visible but small */
        float half_w = eye_r * 0.88f;
        float arc_drop = eye_r * 0.35f;
        float pupil_r = eye_r * 0.26f * (0.5f + ep->pupil_scale * 0.5f);
        if (pupil_r < 2.0f) pupil_r = 2.0f;
        float pupil_r_sq = pupil_r * pupil_r;
        float pupil_dx = ep->iris_center.dx * eye_r * 0.5f;
        float pupil_dy = ep->iris_center.dy * eye_r * 0.5f - eye_r * 0.1f;

        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            if (fabsf(fx) > half_w) continue;
            float t = (fx + half_w) / (2.0f * half_w);
            float lid_y = (1.0f - t) * (1.0f - t) * (eye_cy - arc_drop)
                        + 2.0f * (1.0f - t) * t * (eye_cy + arc_drop * 0.3f)
                        + t * t * (eye_cy - arc_drop);
            /* upper lid arc ~2.5px thick */
            if (fabsf(y - lid_y) < 2.2f) {
                buf[x] = pal[PAL_SCLERA];
            }
            /* pupil within the eye area (below lid) */
            if (y > lid_y && y < eye_cy + eye_r * 0.3f) {
                float p_dx = fx - pupil_dx;
                float p_dy = fy - pupil_dy;
                if (p_dx * p_dx + p_dy * p_dy < pupil_r_sq) {
                    buf[x] = pal[PAL_SCLERA];
                }
            }
        }
        break;
    }
```

- [ ] **Step 8: 编译验证**

```bash
cd /Users/nova/proj/harti && . ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

- [ ] **Step 9: Commit**

```bash
git add components/face_system/sprites/sprite_vector.c
git commit -m "feat: implement 6 new eye type rendering branches

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: 更新 draw_eye_left/right 调用点

**Files:**
- Modify: `components/face_system/sprites/sprite_vector.c:183-195`

- [ ] **Step 1: 更新 call sites 传入 brow_params_t**

```c
static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    eye_type_t et = classify_eye(&st->eye[0], &st->brow[0]);
    draw_eye_vector(y, &st->eye[0], sp->eye_radius, eye_cx, eye_cy, et, sp->pal, buf);
}

static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    eye_type_t et = classify_eye(&st->eye[1], &st->brow[1]);
    draw_eye_vector(y, &st->eye[1], sp->eye_radius, eye_cx, eye_cy, et, sp->pal, buf);
}
```

- [ ] **Step 2: 编译验证**

```bash
cd /Users/nova/proj/harti && . ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

- [ ] **Step 3: Commit**

```bash
git add components/face_system/sprites/sprite_vector.c
git commit -m "fix: update draw_eye_left/right to pass brow_params to classify_eye

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: 调整 face_model.c 表情 eye 参数

**Files:**
- Modify: `components/face_system/face_model.c:80-560`

- [ ] **Step 1: 调整 6 个表情的 eye 参数**

按以下精确位置修改 `EXPRESSION_DEFS[]`：

**HAPPY (index 1)** — top_lid_mid.dy:
```c
// Line 91-92: 将 top_lid_mid 从 {0, -0.08f} 改为 {0, -0.10f}
.top_lid_mid = {0, -0.10f}, .bot_lid_mid = {0, 0.12f},
// Line 96-97: 右眼同步
.top_lid_mid = {0, -0.10f}, .bot_lid_mid = {0, 0.12f},
```

**EXCITED (index 7)** — top_lid_mid.dy + pupil_scale:
```c
// Line 331-332: 将 top_lid_mid 从 {0, -0.1f} 改为 {0, -0.12f}
.top_lid_mid = {0, -0.12f}, .bot_lid_mid = {0, 0.1f},
// 将 pupil_scale 从 0.7f 改为 0.45f
.pupil_scale = 0.45f, .shine_intensity = 1.0f,
// Line 336-337: 右眼同步
.top_lid_mid = {0, -0.12f}, .bot_lid_mid = {0, 0.1f},
.pupil_scale = 0.45f, .shine_intensity = 1.0f,
```

**CONTENT (index 9)** — top_lid_mid.dy + bot_lid_mid.dy:
```c
// Line 411-412: 将 top_lid_mid 从 {0, 0.28f} 改为 {0, -0.05f}
// 将 bot_lid_mid 从 {0, 0.02f} 改为 {0, 0.10f}
.top_lid_mid = {0, -0.05f}, .bot_lid_mid = {0, 0.10f},
// Line 416-417: 右眼同步
.top_lid_mid = {0, -0.05f}, .bot_lid_mid = {0, 0.10f},
```

**COLD (index 10)** — iris_center.dy:
```c
// Line 452: 将 iris_center 从 {0, 0.2f} 改为 {0, 0.28f}
.iris_center = {0, 0.28f},
// Line 457: 右眼同步
.iris_center = {0, 0.28f},
```

**WARM (index 11)** — top_lid_mid.dy:
```c
// Line 491: 将 top_lid_mid 从 {0, -0.03f} 改为 {0, 0.14f}
.top_lid_mid = {0, 0.14f}, .bot_lid_mid = {0, 0.1f},
// Line 496: 右眼同步
.top_lid_mid = {0, 0.14f}, .bot_lid_mid = {0, 0.1f},
```

- [ ] **Step 2: 编译验证**

```bash
cd /Users/nova/proj/harti && . ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

- [ ] **Step 3: Commit**

```bash
git add components/face_system/face_model.c
git commit -m "fix: adjust expression eye params for new 8-type classifier

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## 验证清单

完成所有任务后，验证以下内容：

1. ✅ `idf.py build` 通过（无编译错误/警告）
2. ✅ 13 种情绪各自由正确的眼型分类（对照 spec 映射表）
3. ✅ 现有嘴型不受影响（未修改 mouth 相关代码）
4. ✅ V_EYE_NORMAL 和 V_EYE_SLEEPY 渲染与优化前一致
