# harti — ESP32-S3 表情显示固件

ESP32-S3 + GC9A01 240×240 圆形屏，运行 LVGL，通过 BLE 控制面部表情。

## 环境初始化

每次新终端必须执行：

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh
```

## 编译 & 烧录

```bash
cd /Users/nova/proj/harti
idf.py build
idf.py -p /dev/tty.usbmodem* flash monitor
```

仅编译不烧录：`idf.py build`
仅烧录（已编译）：`idf.py -p /dev/tty.usbmodem* flash`

## 配色规范（严格）

**所有主题表情（全部 9 种 sprite 风格）只能用黑色和白色**，严禁引入任何其他颜色：

- 仅允许 `RGB565(0, 0, 0)` (黑) 和 `RGB565(255, 255, 255)` (白)，不得出现任何第三个颜色值
- `PAL_PUPIL` 使用黑色（便于判断瞳孔看的方向），其余特征统一用白色
- 背景始终为黑色 `RGB565(0, 0, 0)`
- `face_palette.c` 中所有 `PALETTE_*` 的每个条目必须是黑或白，不得包含粉色、蓝色、棕色等任何其他颜色
- 所有 `sprites/sprite_*.c` 的绘制代码中，不得通过 `blend_colors` 或其他方式间接引入黑白以外的颜色

标准调色板参考 `PALETTE_VECTOR`。此规范适用于 vector、classic、lineart、nova、cat、pixel、robot、pig、chibi 全部风格。

## 架构

```
components/face_system/
├── face_model.h/c       # 数据结构 + 13 种表情预设
├── face_animator.h/c    # LVGL 动画系统（lerp 过渡）
├── face_micro.h/c       # 微动画（眨眼、视线游荡、呼吸、倾斜跟踪）
├── face_renderer.h/c    # 逐行渲染 + GC9A01 输出
├── face_palette.h/c     # 配色方案（8 套主题）
├── face_api.h           # 对外的统一 API
├── face_common.h        # 数学/曲线/扫描线绘制工具
└── sprites/
    ├── sprite_registry.c/h  # Sprite 注册表
    ├── sprite_vector.c/h    # ★ VECTOR 风格（主要使用的风格）
    ├── sprite_classic.c/h   # 经典风格
    ├── sprite_lineart.c/h   # 线稿风格
    ├── sprite_nova.c/h      # Nova 风格（白脸 + 黑五官）
    ├── sprite_cat.c/h       # 猫风格
    ├── sprite_pixel.c/h     # 像素风格
    ├── sprite_robot.c/h     # 机器人风格
    └── sprite_pig.c/h       # 猪风格
```

## 表情系统约定

- 表情通过 `expression_id_t`（0-12 共 13 种）索引 `EXPRESSION_DEFS[]`
- 每个表情定义 target 状态 + 每个 component 的过渡时间/缓动
- `classify_mouth()` 根据 mouth_params + eye_params 自动选择嘴巴绘制类型（V_SMILE、V_CONFUSED 等）
- 添加新表情类型时，需同时在 `face_model.c`（参数）和 `sprite_*.c`（渲染）中实现
- 微动画（`face_micro.c`）不应与表情类型耦合，它们是无状态的叠加效果

## 关键文件位置

- 表情定义：`components/face_system/face_model.c` → `EXPRESSION_DEFS[]`
- VECTOR 渲染：`components/face_system/sprites/sprite_vector.c`
- 配色定义：`components/face_system/face_palette.c`
- BLE 控制：`main/app_ble.c`
- 主循环：`main/main.c`
