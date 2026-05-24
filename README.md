# Harti - 桌面互动宠物

基于 ESP32-S3 + GC9A01 240x240 圆形屏幕的桌面互动宠物项目。

## 硬件连接

### 完整 GPIO 资源映射

| Pin | GPIO | Functions | Harti Usage |
|-----|------|-----------|-------------|
| 1 | - | 3V3 | Power |
| 2 | - | 3V3 | Power |
| 3 | - | RST | Reset |
| 4 | GPIO4 | ADC1_3, TOUCH4, RTC | - |
| 5 | GPIO5 | ADC1_4, TOUCH5, RTC | - |
| 6 | GPIO6 | ADC1_5, TOUCH6, RTC | - |
| 7 | GPIO7 | ADC1_6, TOUCH7, RTC | - |
| 8 | GPIO2 | JTAG, ADC1_2, TOUCH3, RTC | - |
| 9 | GPIO15 | ADC2_4, XTAL_32K_N, U0CTS, RTC | - |
| 10 | GPIO16 | ADC2_5, XTAL_32K_P, U0RTS, RTC | - |
| 11 | GPIO2 | JTAG, ADC1_2, TOUCH3, RTC | - |
| 12 | GPIO46 | LOG | - |
| 13 | GPIO9 | FSPIHD, SUBSPIHD, ADC1_8, TOUCH9, RTC | **I2C SCL** (MPU6050 IMU) |
| 14 | GPIO10 | FSPICS0, SUBSPI_CS0, FSPIIO4, ADC1_9, TOUCH10, RTC | - |
| 15 | GPIO11 | FSPID, SUBSPID, FSPIIO5, ADC2_0, TOUCH11, RTC | - |
| 16 | GPIO12 | FSPICLK, SUBSPICLK, FSPIIO6, ADC2_1, TOUCH12, RTC | - |
| 17 | GPIO13 | FSPIQ, SUBSPIQ, FSPIIO7, ADC2_2, TOUCH13, RTC | - |
| 18 | GPIO14 | FSPIWP, SUBSPIWP, FSPIIDQS, ADC2_3, TOUCH14, RTC | - |
| 19 | - | GND | Ground |
| 20 | GPIO43 | U0TXD, CLK_OUT1 | - |
| 21 | GPIO44 | U0RXD, CLK_OUT2 | - |
| 22 | GPIO1 | RTC, TOUCH1, ADC1_0 | **TOUCH0** (head touch) |
| 23 | GPIO2 | RTC, TOUCH2, ADC1_1 | - |
| 24 | GPIO42 | MTMS | **LCD RST** (GC9A01 reset) |
| 25 | GPIO41 | MTDI, CLK_OUT1 | **LCD DC** (GC9A01 data/cmd) |
| 26 | GPIO40 | MTDO, CLK_OUT2 | **LCD CS** (GC9A01 SPI CS) |
| 27 | GPIO39 | MTCK, CLK_OUT3, SUBSPI_CS1 | - |
| 28 | GPIO38 | FSPIWP, SUBSPIWP | - |
| 29 | GPIO37 | FSPIQ, SUBSPIQ | - |
| 30 | GPIO36 | SPIIO7, FSPICLK, SUBSPICLK | **LCD SCK** (GC9A01 SPI clock) |
| 31 | GPIO35 | SPIIO6, FSPID, SUBSPID | **LCD SDA** (GC9A01 SPI MOSI) |
| 32 | GPIO0 | BOOT | **ADC1_CH0** (NTC temp) |
| 33 | GPIO45 | VSPI | - |
| 34 | GPIO48 | SPICLK_L_P, RGB LED | - |
| 35 | GPIO47 | SPICLK_L_N | - |
| 36 | GPIO21 | RTC | - |
| 37 | GPIO20 | USB_D+, RTC, U1CTS, ADC2_9, CLK_OUT1 | - |
| 38 | GPIO19 | USB_D-, RTC, U1RTS, ADC2_8, CLK_OUT2 | - |
| 39 | - | GND | Ground |
| 40 | - | GND | Ground |

| GPIO | Functions | Harti Usage |
|------|-----------|-------------|
| GPIO8 | ADC1_7, TOUCH8, RTC | **I2C SDA** (MPU6050 IMU) |

### 已使用 GPIO 汇总

| GPIO | Function | Connected To | Defined In |
|------|----------|-------------|------------|
| GPIO0 | ADC1_CH0 | NTC thermistor (temperature) | `components/harti_temp/harti_temp.c:15-16` |
| GPIO1 | TOUCH0 | Head touch copper foil | `main/app_sensors.c:18` |
| GPIO8 | I2C SDA | MPU6050 IMU | `main/app_sensors.c:17` |
| GPIO9 | I2C SCL | MPU6050 IMU | `main/app_sensors.c:16` |
| GPIO35 | SPI MOSI | GC9A01 LCD (SDA) | `components/gc9a01/gc9a01.c:13` |
| GPIO36 | SPI SCK | GC9A01 LCD (SCK) | `components/gc9a01/gc9a01.c:14` |
| GPIO40 | SPI CS | GC9A01 LCD (CS) | `components/gc9a01/gc9a01.c:17` |
| GPIO41 | GPIO Out | GC9A01 LCD (DC) | `components/gc9a01/gc9a01.c:16` |
| GPIO42 | GPIO Out | GC9A01 LCD (RST) | `components/gc9a01/gc9a01.c:15` |

> **Notes**: Backlight (BLK) hardwired to VCC, disabled in code (`PIN_BLK = -1`). SPI: `SPI2_HOST` @ 10MHz, 3-wire. I2C: `I2C_NUM_0`, internal pull-ups. IMU: MPU6050 @ 0x68, ±8g/±500dps/100Hz ODR.

## 开发环境设置

### 1. 安装 ESP-IDF

```bash
# 克隆 ESP-IDF
cd ~
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v6.0.1
./install.sh esp32s3
```

### 2. 设置环境变量

每次打开新终端需要运行：
```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh
```

### 3. 编译项目

```bash
cd /path/to/harti
idf.py build
idf.py -p /dev/tty.usbmodem* flash monitor
```

## Git 自动编译验证

项目已配置 `pre-commit` 钩子，每次 `git commit` 前会自动编译验证代码正确性。

如果 ESP-IDF 环境未设置，会跳过验证并提示。

## 项目结构

```
harti/
├── main/
│   ├── main.c              # 入口，创建 FreeRTOS 任务
│   ├── harti_config.h      # 统一配置 (常量/阈值/类型)
│   ├── app_sensors.c/h     # 传感器事件检测 (IMU/触摸/温度)
│   ├── app_behavior.c/h    # 行为状态机 + 关系管理
│   ├── app_input.c/h       # 物理输入 (按键 + sprite 切换)
│   ├── app_ble.c/h         # BLE 碰一碰通信 (stub)
│   └── CMakeLists.txt
├── components/
│   ├── face_system/        # 面部表情渲染引擎
│   │   ├── face_api.h         # 公共接口 (应用层唯一入口)
│   │   ├── face_model.h/c     # 数据模型 + 13 种表情预设
│   │   ├── face_animator.h/c  # 基于 LVGL 的动画引擎
│   │   ├── face_renderer.h/c  # 扫描线渲染器
│   │   ├── face_palette.h/c   # 调色板定义
│   │   ├── face_common.h     # 内部工具函数
│   │   └── sprites/           # sprite 资产
│   │       ├── sprite_registry.h/c  # [唯一真相源] 管理所有 sprite
│   │       ├── sprite_vector.c/h    # 矢量风 (默认)
│   │       ├── sprite_lineart.c/h   # 线稿风
│   │       ├── sprite_classic.c/h   # 经典风
│   │       ├── sprite_cat.c/h       # 猫猫风
│   │       ├── sprite_pixel.c/h     # 像素风
│   │       └── sprite_robot.c/h     # 机器人风
│   ├── gc9a01/             # GC9A01 屏幕驱动 (SPI)
│   ├── harti_imu/          # IMU 驱动 (MPU6050, I2C)
│   └── harti_temp/         # 温度传感器驱动 (NTC + ADC)
├── docs/
│   ├── ARCHITECTURE.md     # 软件架构
│   ├── HARDWARE.md         # 硬件清单
│   ├── INTERACTION.md      # 交互设计
│   ├── PRODUCT.md          # 产品定义
│   ├── resources.md        # GPIO 资源映射
│   └── assets/             # 产品图片 & 面部预览
└── CMakeLists.txt
```

## 面部表情系统

四层组件化架构，扫描线渲染（每行 ~480 字节缓冲）：

| 层 | 文件 | 职责 |
|----|------|------|
| **Face API** | `face_api.h` | 统一对外接口，封装所有内部细节 |
| **Face Animator** | `face_animator.h/c` | 基于 LVGL `lv_anim_t` 的逐组件动画插值 |
| **Face Renderer** | `face_renderer.h/c` | 8-pass z-order 扫描线合成器 |
| **Face Model** | `face_model.h/c` | 7 组件参数结构体 + 13 种表情预设 |

### 8-pass 渲染层级 (z-order)
```
face → blush → mouth → eyes → brows → decor overlay
```

### 精灵 (4 套)
| 精灵 | 调色板 | 特点 |
|------|--------|------|
| **Classic** | 白色/黑色 | 圆形眼睛，二次贝塞尔眉毛，舌头渲染 |
| **Cat** | 暗色+金色 | 竖缝瞳孔，三角猫耳，Ω 形嘴，胡须 |
| **Pixel** | 高亮 8-bit | 4px 网格量化，纯色无反锯齿 |
| **Robot** | 金属蓝 | 六角形眼眶，LED 辉光，面板线 |

### 微动画 (5 种)
| 动画 | 描述 |
|------|------|
| Blink + Wink | 眨眼 (1/5 概率单眼) |
| Breath | 面部圆润度振荡 |
| Saccade | 虹膜微动 + 随机大幅跳变 |
| Brow Twitch | 左右眉毛独立微颤 |
| Mouth Idle | 嘴巴间歇开合 |

### 表情列表 (13 种)

| 表情 | 触发方式 |
|------|---------|
| NEUTRAL - 中性 | 默认 |
| HAPPY - 开心 | 摸头 |
| SAD - 难过 | 连拍 |
| SURPRISED - 惊讶 | 摇晃 / 翻转 |
| SLEEPY - 困倦 | 30s 无互动 |
| ANGRY - 生气 | (预留) |
| BORED - 无聊 | 10s 无互动 |
| EXCITED - 兴奋 | (预留) |
| CONFUSED - 困惑 | 摇晃后恢复 |
| CONTENT - 满足 | 持续摸头 |
| COLD - 冷 | 温度 < 15°C |
| WARM - 温暖 | 被捂热 |
| HEART_EYES - 心心眼 | 挚友碰面 |

### 预览

打开 `docs/assets/face-preview.html` 可在浏览器中预览所有表情、精灵和微动画效果。

## 内存占用

扫描线渲染，仅使用 **~480 字节** 行缓冲 + 面部状态结构体，无全屏帧缓冲。
