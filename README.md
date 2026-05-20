# Harti - 桌面互动宠物

基于 ESP32-S3 + GC9A01 240x240 圆形屏幕的桌面互动宠物项目。

## 硬件连接

| GC9A01 | ESP32-S3 |
|--------|----------|
| SDA/MOSI | GPIO 35 |
| SCK | GPIO 36 |
| RES | GPIO 42 |
| DC | GPIO 41 |
| CS | GPIO 40 |
| BLK | 未使用 (VCC 供电) |

其他外设:
| 外设 | ESP32-S3 |
|------|----------|
| I2C SDA | GPIO 8 |
| I2C SCL | GPIO 9 |
| TOUCH (头部) | GPIO 1 |
| NTC ADC | GPIO 0 |

详见 `docs/resources.md` 和 `docs/HARDWARE.md`。

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
│   ├── app_display.c/h     # 表情渲染 + 微动画
│   ├── app_sensors.c/h     # 传感器事件检测 (IMU/触摸/温度)
│   ├── app_behavior.c/h    # 行为状态机 + 关系管理
│   ├── app_effects.c/h     # 特效渲染 (星星/心心/彩虹/金光)
│   ├── app_ble.c/h         # BLE 碰一碰通信 (stub)
│   └── CMakeLists.txt
├── components/
│   ├── gc9a01/             # GC9A01 屏幕驱动 (SPI)
│   ├── expressive_eyes/    # 表情渲染引擎 (极低内存, ~480 字节)
│   ├── harti_imu/          # IMU 驱动 (MPU6050, I2C)
│   └── harti_temp/         # 温度传感器驱动 (NTC + ADC)
├── docs/
│   ├── ARCHITECTURE.md     # 软件架构
│   ├── HARDWARE.md         # 硬件清单
│   ├── INTERACTION.md      # 交互设计
│   ├── PRODUCT.md          # 产品定义
│   ├── resources.md        # GPIO 资源映射
│   └── assets/             # 产品图片
└── CMakeLists.txt
```

## 表情列表 (13 种)

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

## 内存占用

仅使用 **~480 字节** 行缓冲，无全屏帧缓冲！
