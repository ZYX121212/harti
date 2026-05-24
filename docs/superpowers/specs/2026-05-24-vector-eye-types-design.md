# Vector Eye Types Design

## Summary

将 vector sprite 的眼型从 2 种（NORMAL / SLEEPY）扩展到 8 种，每种情绪有匹配的眼型，提升表情区分度。嘴型保持不变。

## Decision Log

| Decision | Choice | Rationale |
|----------|--------|-----------|
| 眼型数量 | 8 种 | 足够覆盖 13 种情绪，每种情绪至少一种匹配眼型 |
| 分类方式 | `classify_eye()` 阈值判断 | 沿用现有模式，基于 eye_params_t 字段区分 |
| 嘴巴 | 不动 | 当前 10 种嘴型已够用，舌头/牙齿效果不理想 |
| 其他 sprite | 各自实现 | 每个 sprite 有独立视觉风格，需单独适配 |
| NEUTRAL 眼型 | 圆形眼 | 用 iris_center 偏移区分 NEUTRAL 和 CONFUSED（后者不对称） |

---

## Eye Type Enum

```c
typedef enum {
    V_EYE_NORMAL,      // ① 圆眼 — NEUTRAL, CONFUSED
    V_EYE_HAPPY,       // ② 弯月眼 — HAPPY, CONTENT
    V_EYE_SURPRISED,   // ③ 大圆眼（低 pupil_scale + 宽睑裂）— SURPRISED, EXCITED
    V_EYE_SAD,         // ④ 垂眼（下眼睑下垂 + 瞳孔下移）— SAD, COLD
    V_EYE_HEART,       // ⑤ 心形瞳（瞳孔区域改成心形）— HEART_EYES
    V_EYE_ANGRY,       // ⑥ 锐角眼（上眼睑呈倒 V）— ANGRY
    V_EYE_BORED,       // ⑦ 半垂眼（上眼睑半覆盖）— BORED, WARM
    V_EYE_SLEEPY,      // ⑧ 横线眼（保留现有横线逻辑）— SLEEPY
} eye_type_t;
```

---

## Classification Rules (`classify_eye`)

```c
static eye_type_t classify_eye(const eye_params_t *ep, const brow_params_t *bp) {
    // ⑧ SLEEPY: top lid heavily drooped (keep existing threshold)
    if (ep->top_lid_mid.dy > 0.22f) return V_EYE_SLEEPY;

    // ⑤ HEART: pupil scaled to near-zero — heart replaces pupil
    if (ep->pupil_scale < 0.05f) return V_EYE_HEART;

    // ⑥ ANGRY: thick brows + inner brows pulled down (angry brow shape)
    if (bp->thickness > 1.2f && bp->inner.dy < -0.12f) return V_EYE_ANGRY;

    // ③ SURPRISED: wide-open eye (top lid up, bot lid down, small pupil)
    if (ep->top_lid_mid.dy < -0.10f && ep->pupil_scale < 0.48f) return V_EYE_SURPRISED;

    // ④ SAD: pupil sinks downward (following droopy mood)
    if (ep->iris_center.dy > 0.25f) return V_EYE_SAD;

    // ⑦ BORED: half-lidded (top lid droops but not fully sleepy)
    if (ep->top_lid_mid.dy > 0.12f) return V_EYE_BORED;

    // ② HAPPY: bottom lid pushed up (squint) + top lid slightly down
    if (ep->bot_lid_mid.dy > 0.08f && ep->top_lid_mid.dy < -0.04f) return V_EYE_HAPPY;

    // ① NORMAL: default round eye
    return V_EYE_NORMAL;
}
```

`classify_eye` 现在接受 `brow_params_t *` 参数，因为 ANGRY 眼型需要眉毛信息来区分。

---

## Drawing Specification

### ① NORMAL (圆眼)

保持现有 V_EYE_NORMAL 绘制：
- 白色圆形外圈 (~2.5px)
- 白色圆形瞳孔（位置由 iris_center 偏移）
- limbal ring + dual catchlight（由 iris_detail / shine_intensity 控制）
- 睫毛由 eyelash 控制

与现有代码一致，但作为显式分支。

### ② HAPPY (弯月眼)

- 白色上弧线：`path Q(x-18, y+8) → (cx, y-8) → (x+18, y+8)` — 下弯弧
- 无瞳孔、无外圈、无睫毛
- 弧线粗细 ~2.5px
- bot_lid_mid 上推程度影响弧线弯曲深度

### ③ SURPRISED (大圆眼)

- 白色圆形外圈，但更细 (~1.5px)
- 瞳孔缩小（pupil_r 取 pupil_scale，约 0.45 → ~5px 半径）
- eye_radius 在代码中动态扩展：`float r = eye_r * (1.0f + 0.15f * surprise_factor)`
- 无睫毛（eyelash 抑制）

### ④ SAD (垂眼)

- 椭圆外圈（rx = eye_r, ry = eye_r * 0.85）
- 瞳孔下移（iris_center.dy 驱动）+ 缩小
- limbal ring 抑制（iris_detail 低）
- shine_intensity 降低 → catchlight 缩小

### ⑤ HEART (心形瞳)

- 圆形外圈同 NORMAL
- 瞳孔区域用心形替换：两个上半圆弧 + 底部尖角
- 心形内部填充 PAL_PUPIL（黑），无 catchlight
- 心形路径参考 face_common.h 中已有的 draw_heart_scan（但方向朝上）
- pupil_scale 字段被 HEART_EYES 表达式设为 0.0f 用于分类；心形尺寸用 iris_detail 缩放

### ⑥ ANGRY (锐角眼)

- 上眼睑用 polygon：`(inner_x, mid_y) → (cx, cy - r*0.7) → (outer_x, mid_y) → (cx, cy - r*0.3)` — 倒 V 形状
- 瞳孔位于中心偏上
- 眉毛已由 face_model.c 的 ANGRY 预设驱动（粗 + 内低外高），无需眼型内额外处理

### ⑦ BORED (半垂眼)

- 白色外弧线：上眼睑半覆盖，呈现平弧 `path Q(x-18, y+8) → (cx, y+6) → (x+18, y+8)`
- 瞳孔可见但上移（iris_center.dy 为负）
- 弧线粗细 ~2.5px

### ⑧ SLEEPY (横线眼)

保持现有 V_EYE_SLEEPY 绘制：
- 外圈 + 水平横线覆盖
- 代码不变，仅分类 enum 重命名

---

## Emotion-to-Eye Mapping

| 情绪 | 眼型 | 关键区分参数 |
|------|------|------------|
| NEUTRAL | ① NORMAL | top_lid_mid 接近 0 |
| HAPPY | ② HAPPY | bot_lid_mid.dy > 0.08, top_lid_mid.dy < -0.04 |
| SAD | ④ SAD | iris_center.dy > 0.25 |
| SURPRISED | ③ SURPRISED | top_lid_mid.dy < -0.10, pupil_scale < 0.48 |
| SLEEPY | ⑧ SLEEPY | top_lid_mid.dy > 0.22 |
| ANGRY | ⑥ ANGRY | brow thickness > 1.2, brow inner.dy < -0.12 |
| BORED | ⑦ BORED | top_lid_mid.dy > 0.12 |
| EXCITED | ③ SURPRISED | 同 SURPRISED |
| CONFUSED | ① NORMAL | 同 NORMAL，不对称 iris_center 由表达式数据提供 |
| CONTENT | ② HAPPY | 同 HAPPY，但强度较低（arc 更浅） |
| COLD | ④ SAD | 同 SAD |
| WARM | ⑦ BORED | 同 BORED（放松感） |
| HEART_EYES | ⑤ HEART | pupil_scale < 0.05 |

---

## face_model.c 参数调整

部分表情的 eye 参数需要微调，确保落入正确分类：

| 表情 | 调整字段 | 当前值 | 新值 | 原因 |
|------|---------|--------|------|------|
| HAPPY | top_lid_mid.dy | -0.08 | -0.10 | 确保满足 HAPPY 分类（< -0.04） |
| EXCITED | top_lid_mid.dy | -0.10 | -0.12 | 确保满足 SURPRISED 分类（< -0.10） |
| EXCITED | pupil_scale | 0.70 | 0.45 | 确保满足 SURPRISED 分类（< 0.48） |
| CONTENT | top_lid_mid.dy | 0.28 | -0.05 | 改为 HAPPY 分类，当前会误入 SLEEPY |
| CONTENT | bot_lid_mid.dy | 0.02 | 0.10 | 确保满足 HAPPY 分类（> 0.08） |
| WARM | top_lid_mid.dy | -0.03 | 0.14 | 确保满足 BORED 分类（> 0.12） |
| COLD | iris_center.dy | 0.20 | 0.28 | 确保满足 SAD 分类（> 0.25） |
| HEART_EYES | pupil_scale | 0.0 | 0.0 | 保持不变（已 < 0.05） |

---

## `draw_eye_left` / `draw_eye_right` 调用变更

```c
static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    // ...
    eye_type_t et = classify_eye(&st->eye[0], &st->brow[0]);  // 新增 brow 参数
    draw_eye_vector(y, &st->eye[0], sp->eye_radius, eye_cx, eye_cy, et, sp->pal, buf);
}
```

---

## 其他 Sprite 适配

每个 sprite 需要：
1. 定义自己的 eye_type_t 枚举（或共用 vector 的）
2. 实现 `classify_eye()`（可直接复用 vector 的分类逻辑）
3. 在 `draw_eye_*` 中为 8 种眼型提供视觉实现

各 sprite 的风格差异：
- **classic / lineart / nova / cat / pixel / robot / pig / chibi**：每个风格的眼型外观不同（线条、像素、几何等），但分类逻辑相同

---

## 不在范围内

- 嘴型优化（舌头、牙齿、参数化 SMILE/SAD）
- mouth_params_t 新增字段
- 眉毛分类优化
- 非 vector sprite 的眼型绘制（后续单独 PR）
