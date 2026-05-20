# Phase 2: 精灵增强与新角色

**日期**: 2026-05-20
**状态**: 设计中
**依赖**: Phase 1 (face_system 四层架构已完成)

---

## 动机

Phase 1 建立了组件化面部框架和默认 classic 精灵。Phase 2 在三个方向发力：
1. 增强 classic 精灵的渲染质量（眉毛、嘴巴、眼角）
2. 新增 3 套角色精灵（cat/pixel/robot）
3. 增强微动画层次

---

## A. Classic 精灵增强

### A1. 眉毛锥度 (Brow Taper)

**当前问题**: 眉毛从头到尾粗细一致，不自然。

**改进**:
- 新增参数 `brow_params_t.taper`（0=无锥度，1=最大锥度，默认 0.6）
- 渲染时沿贝塞尔曲线动态计算线宽：`inner=100% → arch=85% → tail=45%`
- 抗锯齿：边缘 alpha 渐变

**影响文件**: `face_model.h` (brow_params_t), `sprite_classic.c` (draw_brow_impl)

### A2. 嘴巴增强

**当前问题**: 开口时只有纯色填充，缺乏层次。

**改进**:
- 开口时（`openness > 0.2`）渲染椭圆形舌头（PAL_TONGUE 色）
- 利用 `left_corner.dy` / `right_corner.dy` 支持不对称嘴角
- 上唇线更粗，下唇线更细，模拟光影

**新增颜色**: PAL_TONGUE (#E57373 粉色)

**影响文件**: `face_palette.h`, `sprite_classic.c` (draw_mouth)

### A3. 眼角关键点利用

**当前问题**: `inner_corner.{dx,dy}` 和 `outer_corner.{dx,dy}` 已在 face_model 中定义但渲染未使用。

**改进**:
- `inner_corner.dy` 影响内眼角上下偏移 → 眼睑公式中的内侧调整
- `outer_corner.dy` 影响外眼角上下偏移 → 眼睑公式中的外侧调整
- 内眼角 dx 影响眼睛宽度

**影响文件**: `sprite_classic.c` (draw_eye_impl)

### A4. 脸型 roundness 调制

**当前问题**: `face_params_t.roundness` 参数存在但背景渐变忽略它。

**改进**:
- `roundness = 0` → 菱形背景（dist = |dx| + |dy|）
- `roundness = 0.5` → 圆形（dist = sqrt(dx² + dy²)）
- `roundness = 1` → 椭圆形
- 在 `dist_sq` 和曼哈顿距离之间混合

**影响文件**: `sprite_classic.c` (build_bg_lut / draw_face)

---

## B. 新增精灵套件

每套精灵提供独立的 `sprite_set_t`、调色板和渲染函数。

### B1. Cat（猫精灵）

**特征**:
- **竖瞳**: 菱形瞳孔，`pupil_scale` 控制狭缝宽度（0=细缝, 1=圆瞳）
- **猫耳朵**: 三角形，位于眉毛上方，随表情可动
- **猫嘴**: "ω" 形（三点式曲线，中间上扬）
- **胡须**: 左右各 3 根，作为 decor 元素
- **调色板 PALETTE_CAT**: 金色虹膜、深灰背景、粉色鼻子

**新增函数**: `draw_cat_ears`（z-order 在最上层，耳朵上方）

### B2. Pixel（像素风精灵）

**特征**:
- 无抗锯齿，blocky 渲染
- 整数像素坐标（`floor` 处理）
- 眼睛: 4×4 像素块组成
- 嘴巴: 2px 宽的线条
- 低分辨率美学，8-bit 风格
- **调色板 PALETTE_PIXEL**: 高对比度、饱和色

### B3. Robot（机械风精灵）

**特征**:
- **眼睛**: 六边形眼眶，LED 发光（中心更亮+光晕）
- **嘴巴**: 矩形栅格纹
- **面板线**: 垂直/水平细线作为 decor
- **机械色调 PALETTE_ROBOT**: 金属灰、蓝色 LED、红色指示灯

---

## C. 微动画增强

### C1. 不对称眨眼 (Wink)

```
检测眨眼波形: 偶尔 (1/5 概率) 只闭单眼
状态: 0=等待, 1=闭左眼, 2=闭右眼, 3=双眼同闭
wink_type 在每次 blink 触发时随机决定
```

### C2. 眉毛微动

```
每帧微小偏移 brow position (0.005 量级)
用正弦波驱动，左右眉可独立相位
仅在不处于表情过渡动画时生效
```

### C3. 空闲嘴部微动

```
在 NEUTRAL 表情时偶尔微张 (openness += 0.02 随机)
概率: 每 3-8 秒触发一次
```

### C4. 眼球跳动随机化

```
saccade 幅度从固定值改为随机 (0.02-0.08)
saccade 方向随机化（不仅正弦轨迹）
偶发性大幅度跳动 (0.1-0.15)
```

---

## 文件结构

```
components/face_system/
├── face_model.h/c          — 修改: brow taper, 新增 sprite extern
├── face_palette.h          — 修改: 新增 PAL_TONGUE, PALETTE_CAT/PIXEL/ROBOT
├── sprite_classic.c        — 修改: A1-A4 增强
├── sprites/
│   ├── sprite_cat.c/h      — 新增: 猫精灵
│   ├── sprite_pixel.c/h    — 新增: 像素精灵
│   └── sprite_robot.c/h   — 新增: 机械精灵
├── face_api.h              — 修改: 新增 face_set_sprite 切换
├── CMakeLists.txt          — 修改: 新增 sprite 源文件
main/
└── app_display.c           — 修改: C1-C4 微动画增强
```

---

## 开放问题

- 猫耳朵的绘制：是否需要在 face_renderer z-order 中新增层级
- pixel 精灵的块状渲染与现有 scanline 混合方式的兼容性
- 内存预算：3 个新精灵各 ~400B（调色板+几何）→ 约 1.2KB 额外 ROM
