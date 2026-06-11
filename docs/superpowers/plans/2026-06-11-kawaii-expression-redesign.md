# 萌系表情形象重绘 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 harti 全部 9 种 sprite 风格的表情形象"萌化"（大白眼 + 大黑瞳 + 双白色星光 + 腮红 + 柔嘴），并加眼睛灵动动画，全程严格黑白、零 schema 改动。

**Architecture:** 两类叠加式增强：(1) 共享眼睛灵动动画在 `face_micro.c` 调制现有 `eye_params_t` 参数（一次实现、9 风格受益）；(2) 形象重绘——新增共享 `draw_kawaii_pupil()` 助手到 `face_common.h`，VECTOR 旗舰完整重绘作参考，其余 8 风格各自套用统一原则并保留身份；腮红在 `face_model.c` 表情定义里集中开启（各 sprite 已渲染 `decor.blush`）。

**Tech Stack:** ESP-IDF v6.0.1，C，LVGL（仅 `lv_tick_get()`），自定义扫描线渲染器，GC9A01 240×240 圆屏。无单元测试框架——**编译即验证门**：`. ~/.espressif/v6.0.1/esp-idf/export.sh >/dev/null 2>&1 && idf.py build`。`-Werror=missing-field-initializers` 启用。

**配色铁律:** 仅 `RGB565(0,0,0)`/`RGB565(255,255,255)`；灰阶（腮红等）一律 `bayer_accept()` 点阵抖动。每个文件提交前 grep 审查无第三色。

---

## 文件结构

| 文件 | 职责 | 任务 |
|------|------|------|
| `components/face_system/face_micro.c` | 眼睛灵动动画（共享层） | Task 1 |
| `components/face_system/face_common.h` | 新增 `draw_kawaii_pupil()` 共享助手 | Task 2 |
| `components/face_system/face_model.c` | 正向表情开启 `decor.blush` | Task 3 |
| `components/face_system/sprites/sprite_vector.c` | VECTOR 旗舰萌系重绘 | Task 4 |
| `.../sprites/sprite_classic.c` | classic 可爱化 | Task 5 |
| `.../sprites/sprite_cat.c` | cat 可爱化 | Task 6 |
| `.../sprites/sprite_pig.c` | pig 可爱化 | Task 7 |
| `.../sprites/sprite_chibi.c` | chibi 可爱化 | Task 8 |
| `.../sprites/sprite_lineart.c` | lineart 可爱化 | Task 9 |
| `.../sprites/sprite_nova.c` | nova 反相可爱化 | Task 10 |
| `.../sprites/sprite_pixel.c` | pixel 像素萌化 | Task 11 |
| `.../sprites/sprite_robot.c` | robot 机械萌化 | Task 12 |

---

## Task 1: 眼睛灵动动画（`face_micro.c`）

**Files:**
- Modify: `components/face_system/face_micro.c`（在状态变量区加 `blink_open_at[2]`；在 `micro_animator_apply()` 末尾、第 7 节 Entry Impact 块之后、函数闭合 `}` 之前，加第 8 节）

- [ ] **Step 1: 加眨眼回弹的状态变量**

在文件状态变量区（紧邻 `static uint32_t next_blink_at;` 那一行之后）新增：

```c
static uint32_t blink_open_at[2] = {0, 0};  /* 眨眼睁开完成时刻，用于回弹 */
```

- [ ] **Step 2: 在眨眼睁开完成处记录时间戳**

在 `micro_animator_apply()` 的 `BLINK_OPENING` 分支里，找到普通眨眼结束转回 `BLINK_WAITING` 的 `else` 块（当前形如）：

```c
                } else {
                    eb->phase = BLINK_WAITING;
                    eb->phase_start = now;
                    eb->double_done = false;
                    eb->winking = false;  // clear manual wink flag
                }
```

改为（仅多加一行记录时间戳）：

```c
                } else {
                    eb->phase = BLINK_WAITING;
                    eb->phase_start = now;
                    eb->double_done = false;
                    eb->winking = false;  // clear manual wink flag
                    blink_open_at[i] = now;  /* 触发眨眼回弹 */
                }
```

- [ ] **Step 3: 在函数末尾加第 8 节眼睛灵动动画**

在 `micro_animator_apply()` 的最后（第 7 节 Entry Impact 的 `}` 之后、函数结束 `}` 之前）插入：

```c
    /* ══════════════════════════════════════════════════════
       8. Eye-liveliness（萌系灵动细节）
       ══════════════════════════════════════════════════════ */

    /* 8.1 高光微闪：星光像水光一样呼吸 */
    {
        float shimmer = sinf(total_s * 2.0f * 3.14159265f * 0.2f) * 0.12f;
        for (int i = 0; i < 2; i++) {
            s->eye[i].shine_intensity =
                clampf(s->eye[i].shine_intensity + shimmer, 0.0f, 1.0f);
        }
    }

    /* 8.2 好奇瞳孔游走：空闲期（无扫视/无眼神交流）慢速李萨如 */
    if (gaze_phase == GAZE_DWELL && ec_phase == EC_IDLE) {
        float dx = sinf(total_s * 2.0f * 3.14159265f * 0.13f) * 0.04f;
        float dy = sinf(total_s * 2.0f * 3.14159265f * 0.17f + 1.0f) * 0.04f;
        for (int i = 0; i < 2; i++) {
            s->eye[i].iris_center.dx += dx;
            s->eye[i].iris_center.dy += dy;
        }
    }

    /* 8.3 星瞳/爱心瞳脉动：DIZZY / HEART_EYES 心跳式放大缩小 */
    if (current_expr_id == EMOTION_DIZZY || current_expr_id == EMOTION_HEART_EYES) {
        float pulse = sinf(total_s * 2.0f * 3.14159265f * 1.6f) * 0.18f;
        for (int i = 0; i < 2; i++) {
            s->eye[i].iris_detail =
                clampf(s->eye[i].iris_detail + pulse, 0.0f, 1.0f);
        }
    }

    /* 8.4 眨眼回弹：睁开瞬间上眼睑过冲到负值（眼睛睁更大）再回落 */
    {
        const uint32_t REBOUND_MS = 120;
        for (int i = 0; i < 2; i++) {
            if (blink_open_at[i] == 0) continue;
            uint32_t dt = now - blink_open_at[i];
            if (dt >= REBOUND_MS) { blink_open_at[i] = 0; continue; }
            float t = (float)dt / (float)REBOUND_MS;     /* 0→1 */
            float overshoot = -0.08f * (1.0f - t);        /* 负=睁更大，线性回 0 */
            s->eye[i].top_lid_mid.dy += overshoot;
        }
    }
```

说明：`clampf`、`total_s`、`gaze_phase`、`ec_phase`、`current_expr_id`、`EMOTION_DIZZY`、`EMOTION_HEART_EYES` 均已在本文件作用域内（`EMOTION_*` 来自已 include 的 `../../main/harti_config.h`）。

- [ ] **Step 4: 编译验证**

Run: `. ~/.espressif/v6.0.1/esp-idf/export.sh >/dev/null 2>&1 && idf.py build 2>&1 | grep -iE "error|Project build complete"`
Expected: 输出 `Project build complete...`，无 `error`。

- [ ] **Step 5: 黑白审查（本任务不涉及绘制颜色，确认无新增颜色常量）**

Run: `git diff components/face_system/face_micro.c | grep -iE "RGB565|0x[0-9a-fA-F]{4}" || echo "no color literals — ok"`
Expected: `no color literals — ok`

- [ ] **Step 6: 提交**

```bash
git add components/face_system/face_micro.c
git commit -m "feat(micro): eye-liveliness — catchlight shimmer, idle pupil drift, star/heart pulse, blink rebound"
```

---

## Task 2: 共享萌系瞳孔助手（`face_common.h`）

**Files:**
- Modify: `components/face_system/face_common.h`（在文件末尾 `#endif` 之前，与其它 `static inline` 助手并列处新增）

- [ ] **Step 1: 新增 `draw_kawaii_pupil()`**

在 `face_common.h` 末尾（最后一个 `static inline` 函数之后、文件结尾的 `#endif` 之前）加入：

```c
/* 萌系亮眼瞳孔：大实心瞳 + 双高光星光。逐扫描线在某只眼的 x 范围内调用。
   (pcx,pcy)=瞳孔中心(屏幕px)，pr=瞳孔半径，shine=0..1 星光强度。
   pupil_col=瞳孔填充色，sparkle_col=星光色（常规风格传 黑瞳/白星，nova 反相）。*/
static inline void draw_kawaii_pupil(int y, int x_start, int x_end,
                                     float pcx, float pcy, float pr, float shine,
                                     uint16_t pupil_col, uint16_t sparkle_col,
                                     uint16_t *buf) {
    if (pr < 1.0f) return;
    float pr_sq = pr * pr;
    float cl1_r  = pr * 0.30f;
    float cl1x   = pcx - pr * 0.32f, cl1y = pcy - pr * 0.34f;
    float cl1_sq = cl1_r * cl1_r;
    float cl2_r  = pr * 0.14f;
    float cl2x   = pcx + pr * 0.26f, cl2y = pcy + pr * 0.24f;
    float cl2_sq = cl2_r * cl2_r;
    for (int x = x_start; x <= x_end; x++) {
        float dx = (float)x - pcx, dy = (float)y - pcy;
        if (dx * dx + dy * dy >= pr_sq) continue;
        float s1x = (float)x - cl1x, s1y = (float)y - cl1y;
        float s2x = (float)x - cl2x, s2y = (float)y - cl2y;
        bool sp1 = (shine > 0.1f && s1x * s1x + s1y * s1y < cl1_sq);
        bool sp2 = (shine > 0.1f && s2x * s2x + s2y * s2y < cl2_sq);
        buf[x] = (sp1 || sp2) ? sparkle_col : pupil_col;
    }
}
```

- [ ] **Step 2: 编译验证（头改动需有 .c 引用才编译；本步只确保语法正确，随 Task 4 一起真正生效）**

Run: `. ~/.espressif/v6.0.1/esp-idf/export.sh >/dev/null 2>&1 && idf.py build 2>&1 | grep -iE "error|Project build complete"`
Expected: `Project build complete...`，无 `error`。

- [ ] **Step 3: 提交**

```bash
git add components/face_system/face_common.h
git commit -m "feat(common): add draw_kawaii_pupil — shared glossy pupil+sparkle helper"
```

---

## Task 3: 集中开启正向表情腮红（`face_model.c`）

**Files:**
- Modify: `components/face_system/face_model.c`（`EXPRESSION_DEFS[]` 中相关表情的 `decor.blush` 目标值）

**背景:** 各 sprite 已渲染 `st->decor.blush`（见 `draw_blush`）。把腮红在表情定义里集中开启即可全风格生效。

- [ ] **Step 1: 定位表情定义里 decor 的设置方式**

Run: `grep -n "blush\|decor\|EMOTION_HAPPY\|EMOTION_CONTENT\|EMOTION_HEART_EYES\|EMOTION_WARM\|\.decor" components/face_system/face_model.c | head -40`
阅读输出，确认 `EXPRESSION_DEFS[]` 中每个表情的 `decor` / `blush` 字段如何赋值（target state 里的 `decor.blush` 浮点值，0=无腮红，1=满）。

- [ ] **Step 2: 给 4 个正向表情设置 blush 目标值**

在 `EXPRESSION_DEFS[]` 中，为下列表情的 target state 设置 `decor.blush`（若该字段已存在则改值，结构按文件现有写法）：
- `EMOTION_HAPPY`   → `decor.blush = 0.7f`
- `EMOTION_CONTENT` → `decor.blush = 0.6f`
- `EMOTION_HEART_EYES` → `decor.blush = 0.9f`
- `EMOTION_WARM`    → `decor.blush = 0.6f`

（具体行号按 Step 1 输出定位；只改这 4 个表情的 blush 目标，其余表情 blush 保持 0。）

- [ ] **Step 3: 编译验证**

Run: `. ~/.espressif/v6.0.1/esp-idf/export.sh >/dev/null 2>&1 && idf.py build 2>&1 | grep -iE "error|Project build complete"`
Expected: `Project build complete...`，无 `error`。

- [ ] **Step 4: 提交**

```bash
git add components/face_system/face_model.c
git commit -m "feat(model): enable blush on positive expressions (happy/content/heart-eyes/warm)"
```

---

## Task 4: VECTOR 旗舰萌系重绘（`sprite_vector.c`）

**Files:**
- Modify: `components/face_system/sprites/sprite_vector.c`（`draw_eye_vector()` 内各眼型；`draw_mouth()` 描边粗细）

**核心:** 把"有瞳孔"的眼型从「白圈+白虹膜+黑点」翻转为「实心白眼盘 + 黑瞳 + 双白星光」，调用 Task 2 的 `draw_kawaii_pupil()`。

- [ ] **Step 1: 重写 `V_EYE_NORMAL` 默认分支为实心白眼+黑瞳+白星光**

在 `draw_eye_vector()` 中，把 `case V_EYE_NORMAL: default:` 整个块（当前从 `float eye_r_sq = eye_r * eye_r;` 到该 case 的 `break;`，含 limbal/catchlight/eyelash 逻辑）替换为：

```c
    case V_EYE_NORMAL:
    default: {
        float eye_r_sq = eye_r * eye_r;
        float pupil_dx = ep->iris_center.dx * eye_r * 0.55f;
        float pupil_dy = ep->iris_center.dy * eye_r * 0.55f;
        float pupil_r  = eye_r * 0.50f * (0.7f + ep->pupil_scale * 0.6f);
        if (pupil_r < 4.0f) pupil_r = 4.0f;
        /* 实心白眼盘 */
        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            if (fx * fx + fy * fy < eye_r_sq) buf[x] = pal[PAL_SCLERA];
        }
        /* 黑瞳 + 双白星光 */
        draw_kawaii_pupil(y, x_start, x_end,
                          eye_cx + pupil_dx, eye_cy + pupil_dy, pupil_r,
                          ep->shine_intensity,
                          pal[PAL_PUPIL], pal[PAL_SCLERA], buf);
        /* 睫毛（保留） */
        if (ep->eyelash > 0.2f) {
            static const float lash_x[3] = { -0.35f, 0.0f, 0.35f };
            for (int i = 0; i < 3; i++) {
                int lx = eye_cx + (int)(lash_x[i] * eye_r);
                if (lx < 0 || lx >= SCREEN_W) continue;
                float lash_y = eye_cy - eye_r * 0.78f;
                if (fabsf(y - lash_y) < 2.0f * ep->eyelash) {
                    for (int dx = -2; dx <= 2; dx++) {
                        int px = lx + dx;
                        if (px >= 0 && px < SCREEN_W) buf[px] = pal[PAL_BROW];
                    }
                }
            }
        }
        break;
    }
```

- [ ] **Step 2: 编译 + 目测确认 NORMAL 眼已变实心白眼黑瞳**

Run: `. ~/.espressif/v6.0.1/esp-idf/export.sh >/dev/null 2>&1 && idf.py build 2>&1 | grep -iE "error|Project build complete"`
Expected: `Project build complete...`，无 `error`。

- [ ] **Step 3: 把 SURPRISED 眼改为实心白眼+小高黑瞳+星光**

把 `case V_EYE_SURPRISED:` 块替换为：

```c
    case V_EYE_SURPRISED: {
        float r_ext = eye_r * 1.15f;
        float eye_r_sq = r_ext * r_ext;
        float pupil_dx = ep->iris_center.dx * r_ext * 0.4f;
        float pupil_dy = ep->iris_center.dy * r_ext * 0.4f - r_ext * 0.1f; /* 略上移 */
        float pupil_r  = eye_r * 0.30f * (0.6f + ep->pupil_scale * 0.5f);
        if (pupil_r < 3.0f) pupil_r = 3.0f;
        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            if (fx * fx + fy * fy < eye_r_sq) buf[x] = pal[PAL_SCLERA];
        }
        draw_kawaii_pupil(y, x_start, x_end,
                          eye_cx + pupil_dx, eye_cy + pupil_dy, pupil_r,
                          ep->shine_intensity, pal[PAL_PUPIL], pal[PAL_SCLERA], buf);
        break;
    }
```

- [ ] **Step 4: 把 SAD 眼改为实心白眼+下沉黑瞳+垂眼睑+星光**

把 `case V_EYE_SAD:` 块替换为：

```c
    case V_EYE_SAD: {
        float ry = eye_r * 0.92f;
        float ry_sq = ry * ry;
        float pupil_dx = ep->iris_center.dx * eye_r * 0.5f;
        float pupil_dy = eye_r * 0.32f + ep->iris_center.dy * eye_r * 0.3f; /* 下沉 */
        float pupil_r  = eye_r * 0.40f * (0.6f + ep->pupil_scale * 0.5f);
        if (pupil_r < 3.0f) pupil_r = 3.0f;
        /* 竖椭圆实心白眼盘 */
        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            if ((fx * fx) / (eye_r * eye_r) + (fy * fy) / ry_sq < 1.0f)
                buf[x] = pal[PAL_SCLERA];
        }
        /* 黑瞳 + 星光 */
        draw_kawaii_pupil(y, x_start, x_end,
                          eye_cx + pupil_dx, eye_cy + pupil_dy, pupil_r,
                          ep->shine_intensity, pal[PAL_PUPIL], pal[PAL_SCLERA], buf);
        /* 上眼睑：黑色弧形遮住眼上缘，制造垂眼 */
        float lid_y = eye_cy - eye_r * 0.30f;
        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            if (fx * fx + fy * fy < eye_r_sq_unused(eye_r) && y < lid_y)
                buf[x] = pal[PAL_BG];
        }
        break;
    }
```

注意：上面 `eye_r_sq_unused(eye_r)` 是占位错误——改用直接比较，正确写法为：

```c
        float er2 = eye_r * eye_r;
        float lid_y = eye_cy - eye_r * 0.30f;
        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            if (fx * fx + fy * fy < er2 && y < lid_y)
                buf[x] = pal[PAL_BG];
        }
```

（把 SAD case 内"上眼睑"那段按此正确版本写；不要使用 `eye_r_sq_unused`。）

- [ ] **Step 5: 把 BORED 眼的瞳孔改用 kawaii 助手**

在 `case V_EYE_BORED:` 块中，保留半睑弧线绘制；把原本画瞳孔的内层循环（`if (y > lid_y && y < eye_cy + eye_r * 0.3f) { ... pupil ... }`）替换为在该 case 末尾调用：

```c
        {
            float pupil_dx = ep->iris_center.dx * eye_r * 0.5f;
            float pupil_dy = ep->iris_center.dy * eye_r * 0.5f - eye_r * 0.1f;
            float pupil_r  = eye_r * 0.30f * (0.6f + ep->pupil_scale * 0.5f);
            if (pupil_r < 3.0f) pupil_r = 3.0f;
            draw_kawaii_pupil(y, x_start, x_end,
                              eye_cx + pupil_dx, eye_cy + pupil_dy, pupil_r,
                              ep->shine_intensity, pal[PAL_PUPIL], pal[PAL_SCLERA], buf);
        }
```

（删除原 case 内手写的 pupil 像素循环，避免重复绘制。）

- [ ] **Step 6: 嘴形柔化——加粗 SMILE / SAD / ANGRY 弧线环宽度**

在 `draw_mouth()` 中，把 `V_SMILE`、`V_SAD`、`V_ANGRY` 三个 case 里 `in_arc_ring(... , outer_r, inner_r, ...)` 的内外半径差从 5px（如 `18,13`）加宽到 7px（改为 `19,12`），让嘴更饱满。逐个 case 把 `18, 13` 改为 `19, 12`、`16, 11` 改为 `17, 10`。

- [ ] **Step 7: 编译验证**

Run: `. ~/.espressif/v6.0.1/esp-idf/export.sh >/dev/null 2>&1 && idf.py build 2>&1 | grep -iE "error|Project build complete"`
Expected: `Project build complete...`，无 `error`。

- [ ] **Step 8: 黑白审查**

Run: `git diff components/face_system/sprites/sprite_vector.c | grep -iE "RGB565|0x[0-9a-fA-F]{4}" || echo "no color literals — ok"`
Expected: `no color literals — ok`（只用 `pal[PAL_*]`，无裸颜色）。

- [ ] **Step 9: 提交**

```bash
git add components/face_system/sprites/sprite_vector.c
git commit -m "feat(vector): kawaii redesign — solid white eyes, black pupils, white sparkles, softer mouths"
```

---

## Task 5–12: 其余 8 风格可爱化扫描

> 每个风格独立一任务，结构相同：① 读文件定位眼/嘴绘制 → ② 按该风格的萌化做法改写 → ③ 编译 → ④ 黑白审查 → ⑤ 提交。
> 平滑风格（classic/cat/pig/chibi）直接复用 `draw_kawaii_pupil()`；lineart/nova/pixel/robot 按各自约束改写。

### Task 5: classic（`sprite_classic.c`）

**Files:** Modify `components/face_system/sprites/sprite_classic.c`

- [ ] **Step 1: 定位眼睛绘制**
  Run: `grep -n "PAL_SCLERA\|PAL_PUPIL\|eye_r\|pupil\|catchlight\|shine\|draw_eye\|case V_EYE" components/face_system/sprites/sprite_classic.c`
  阅读眼睛绘制函数，确认与 VECTOR 同构（实心眼盘 + 瞳孔）。

- [ ] **Step 2: 套用 kawaii 瞳孔**
  在该文件顶部确认已 `#include "face_common.h"`（若无则加）。把"普通/惊讶/难过/无聊"等有瞳孔眼型的瞳孔绘制替换为：先填实心白眼盘，再调用
  ```c
  draw_kawaii_pupil(y, x_start, x_end, pcx, pcy, pupil_r,
                    ep->shine_intensity, pal[PAL_PUPIL], pal[PAL_SCLERA], buf);
  ```
  （`pcx/pcy/pupil_r/x_start/x_end` 用该文件已有的眼心、瞳孔半径与扫描范围变量；`pupil_r` 取 `eye_r * 0.5`，最小 4px。）嘴形描边各 case 加宽约 2px。

- [ ] **Step 3: 编译**
  Run: `. ~/.espressif/v6.0.1/esp-idf/export.sh >/dev/null 2>&1 && idf.py build 2>&1 | grep -iE "error|Project build complete"`
  Expected: `Project build complete...`。

- [ ] **Step 4: 黑白审查**
  Run: `git diff components/face_system/sprites/sprite_classic.c | grep -iE "RGB565|0x[0-9a-fA-F]{4}" || echo "ok"`
  Expected: `ok`。

- [ ] **Step 5: 提交**
  ```bash
  git add components/face_system/sprites/sprite_classic.c
  git commit -m "feat(classic): kawaii pass — glossy black pupils + white sparkles"
  ```

### Task 6: cat（`sprite_cat.c`）

**Files:** Modify `components/face_system/sprites/sprite_cat.c`

- [ ] **Step 1: 定位眼睛/胡须绘制**
  Run: `grep -n "PAL_SCLERA\|PAL_PUPIL\|eye_r\|pupil\|whisker\|shine\|case V_EYE" components/face_system/sprites/sprite_cat.c`

- [ ] **Step 2: 套用 kawaii 瞳孔（竖椭圆瞳）+ 放大眼**
  确认 `#include "face_common.h"`。眼睛填实心白盘后调用 `draw_kawaii_pupil(...)`，瞳孔用**竖椭圆**：调用前把 `pupil_r` 用于横向、纵向乘 1.4 模拟竖瞳——若助手只接受圆瞳，则在调用后用该文件已有方式纵向拉伸瞳孔（保留猫竖瞳特征）；星光照常。胡须/耳朵线条无需改色。保留猫脸轮廓。

- [ ] **Step 3: 编译** — Run 同上模板，Expected `Project build complete...`。
- [ ] **Step 4: 黑白审查** — `git diff .../sprite_cat.c | grep -iE "RGB565|0x[0-9a-fA-F]{4}" || echo "ok"` → `ok`。
- [ ] **Step 5: 提交**
  ```bash
  git add components/face_system/sprites/sprite_cat.c
  git commit -m "feat(cat): kawaii pass — glossy vertical-slit pupils + sparkles"
  ```

### Task 7: pig（`sprite_pig.c`）

**Files:** Modify `components/face_system/sprites/sprite_pig.c`

- [ ] **Step 1: 定位** — Run: `grep -n "PAL_SCLERA\|PAL_PUPIL\|eye_r\|pupil\|snout\|nose\|shine\|case V_EYE" components/face_system/sprites/sprite_pig.c`
- [ ] **Step 2: 套用 kawaii 瞳孔** — 确认 `#include "face_common.h"`；实心白眼盘 + `draw_kawaii_pupil(...)`（圆瞳，`pupil_r=eye_r*0.5`）；保留猪鼻绘制不动，鼻孔可略圆润。
- [ ] **Step 3: 编译** — 同模板，Expected `Project build complete...`。
- [ ] **Step 4: 黑白审查** — `git diff .../sprite_pig.c | grep -iE "RGB565|0x[0-9a-fA-F]{4}" || echo "ok"` → `ok`。
- [ ] **Step 5: 提交**
  ```bash
  git add components/face_system/sprites/sprite_pig.c
  git commit -m "feat(pig): kawaii pass — glossy pupils + sparkles, keep snout"
  ```

### Task 8: chibi（`sprite_chibi.c`）

**Files:** Modify `components/face_system/sprites/sprite_chibi.c`

- [ ] **Step 1: 定位** — Run: `grep -n "PAL_SCLERA\|PAL_PUPIL\|eye_r\|pupil\|shine\|catchlight\|case V_EYE" components/face_system/sprites/sprite_chibi.c`
- [ ] **Step 2: 强化双星光** — 确认 `#include "face_common.h"`；有瞳孔眼型用 `draw_kawaii_pupil(...)` 统一双星光；眼半径已大，仅微调瞳孔比例到 `eye_r*0.52`。
- [ ] **Step 3: 编译** — 同模板。
- [ ] **Step 4: 黑白审查** — `git diff .../sprite_chibi.c | grep -iE "RGB565|0x[0-9a-fA-F]{4}" || echo "ok"` → `ok`。
- [ ] **Step 5: 提交**
  ```bash
  git add components/face_system/sprites/sprite_chibi.c
  git commit -m "feat(chibi): kawaii pass — unified dual sparkles"
  ```

### Task 9: lineart（`sprite_lineart.c`）

**Files:** Modify `components/face_system/sprites/sprite_lineart.c`

- [ ] **Step 1: 定位** — Run: `grep -n "PAL_SCLERA\|PAL_PUPIL\|stroke\|outline\|eye_r\|pupil\|case V_EYE" components/face_system/sprites/sprite_lineart.c`
- [ ] **Step 2: 线稿萌化（不填实）** — 保留线条风格：眼眶画更圆润；瞳孔用**实心黑小圆**但在左上留一个**白色高光缺口**（在黑瞳里画一个小白圆，半径约瞳孔 0.3，偏左上）。不调用 `draw_kawaii_pupil`（那是填实风格）；改为该文件现有线条方式 + 手画一个白色高光小圆：
  ```c
  /* 黑瞳内左上白高光缺口 */
  float gx = pcx - pr * 0.32f, gy = pcy - pr * 0.34f, gr = pr * 0.3f;
  for (int x = x_start; x <= x_end; x++) {
      float dx = x - gx, dy = y - gy;
      if (dx*dx + dy*dy < gr*gr) buf[x] = pal[PAL_SCLERA];
  }
  ```
  （`pcx/pcy/pr` 用该文件瞳孔变量。）
- [ ] **Step 3: 编译** — 同模板。
- [ ] **Step 4: 黑白审查** — `git diff .../sprite_lineart.c | grep -iE "RGB565|0x[0-9a-fA-F]{4}" || echo "ok"` → `ok`。
- [ ] **Step 5: 提交**
  ```bash
  git add components/face_system/sprites/sprite_lineart.c
  git commit -m "feat(lineart): kawaii pass — rounder eyes + white catchlight notch"
  ```

### Task 10: nova（白脸黑五官，反相）（`sprite_nova.c`）

**Files:** Modify `components/face_system/sprites/sprite_nova.c`

- [ ] **Step 1: 定位 + 确认调色板反相** — Run: `grep -n "PAL_SCLERA\|PAL_PUPIL\|PAL_BG\|PAL_FACE\|eye_r\|pupil\|case V_EYE" components/face_system/sprites/sprite_nova.c`
  确认 nova 是白脸黑五官（眼睛是脸上的黑形，背景/脸为白）。
- [ ] **Step 2: 反相 kawaii 瞳孔** — 确认 `#include "face_common.h"`；调用助手时**交换颜色**——瞳孔填 `pal[PAL_PUPIL]`（黑），星光用脸色/白：
  ```c
  draw_kawaii_pupil(y, x_start, x_end, pcx, pcy, pupil_r,
                    ep->shine_intensity,
                    pal[PAL_PUPIL] /*黑瞳*/, pal[PAL_SCLERA] /*白星光*/, buf);
  ```
  确认眼睛底是黑（黑瞳在白脸上），星光是白点。若该风格眼睛本就是黑实心，则直接在黑眼内调用助手画白星光即可（pupil_col 传与眼同色的黑、sparkle 传白）。
- [ ] **Step 3: 编译** — 同模板。
- [ ] **Step 4: 黑白审查** — `git diff .../sprite_nova.c | grep -iE "RGB565|0x[0-9a-fA-F]{4}" || echo "ok"` → `ok`。
- [ ] **Step 5: 提交**
  ```bash
  git add components/face_system/sprites/sprite_nova.c
  git commit -m "feat(nova): kawaii pass — white sparkle notch in black pupils (inverted)"
  ```

### Task 11: pixel（`sprite_pixel.c`）

**Files:** Modify `components/face_system/sprites/sprite_pixel.c`

- [ ] **Step 1: 定位像素网格绘制** — Run: `grep -n "PAL_SCLERA\|PAL_PUPIL\|block\|grid\|cell\|px\|pupil\|case V_EYE" components/face_system/sprites/sprite_pixel.c`
  确认像素块单元尺寸（如每格 N px）。
- [ ] **Step 2: 像素萌化（全方块）** — 不用 `draw_kawaii_pupil`（曲线不符像素风）。改为：瞳孔=居中黑色方块（比现状大 1 格）；在黑瞳左上角设**一个白色像素格**作星光；正向表情腮红用棋盘格（隔格白）。用该文件已有的"点格"绘制函数实现，单元对齐网格。
- [ ] **Step 3: 编译** — 同模板。
- [ ] **Step 4: 黑白审查** — `git diff .../sprite_pixel.c | grep -iE "RGB565|0x[0-9a-fA-F]{4}" || echo "ok"` → `ok`。
- [ ] **Step 5: 提交**
  ```bash
  git add components/face_system/sprites/sprite_pixel.c
  git commit -m "feat(pixel): kawaii pass — bigger block pupils + single-pixel sparkle"
  ```

### Task 12: robot（`sprite_robot.c`）

**Files:** Modify `components/face_system/sprites/sprite_robot.c`

- [ ] **Step 1: 定位屏幕眼/天线绘制** — Run: `grep -n "PAL_SCLERA\|PAL_PUPIL\|screen\|antenna\|eye_r\|pupil\|case V_EYE" components/face_system/sprites/sprite_robot.c`
- [ ] **Step 2: 机械萌化** — 屏幕眼角更圆润；在眼内加**一个反光像素高光**（左上小白块，复用 pixel 思路或画 2px 白方）；保留机械外框/天线。可选：用 `draw_kawaii_pupil` 画圆瞳+星光（若机器人眼是圆屏）。
- [ ] **Step 3: 编译** — 同模板。
- [ ] **Step 4: 黑白审查** — `git diff .../sprite_robot.c | grep -iE "RGB565|0x[0-9a-fA-F]{4}" || echo "ok"` → `ok`。
- [ ] **Step 5: 提交**
  ```bash
  git add components/face_system/sprites/sprite_robot.c
  git commit -m "feat(robot): kawaii pass — rounder screen-eyes + reflection glint"
  ```

---

## 完成后

全部任务完成且每步编译通过后：
- announce 使用 `superpowers:finishing-a-development-branch` 完成本分支（合并/PR/保留/丢弃）。

---

## Self-Review 备注

- **Spec 覆盖:** 第 1 部分动画→Task 1；共享助手→Task 2；腮红→Task 3；VECTOR→Task 4；其余 8 风格→Task 5–12。全覆盖。
- **类型一致:** `draw_kawaii_pupil(int y,int x_start,int x_end,float pcx,float pcy,float pr,float shine,uint16_t pupil_col,uint16_t sparkle_col,uint16_t *buf)` 在 Task 2 定义，Task 4–12 调用签名一致。
- **已知不可避免的"读后改":** Task 5–12 涉及尚未通读的 8 个 sprite 文件，每任务 Step 1 强制先 `grep` 定位再改；改写规则与数值均已具体给出，符合各风格约束。
