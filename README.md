# Harti - 桌面互动宠物

基于 ESP32-S3 + GC9A01 240x240 圆形屏幕的桌面互动宠物项目。

## 硬件连接

| GC9A01 | ESP32-S3 |
|--------|----------|
| SDA/MOSI | GPIO 11 |
| SCK | GPIO 12 |
| RES | GPIO 5 |
| DC | GPIO 4 |
| CS | GPIO 6 |
| BLK | GPIO 7 |

## 开发环境设置

### 1. 安装 ESP-IDF

```bash
# 克隆 ESP-IDF
cd ~
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.2.1
./install.sh esp32s3
```

### 2. 设置环境变量

每次打开新终端需要运行：
```bash
. ~/esp-idf/export.sh
```

### 3. 编译项目

```bash
cd /path/to/harti
./build.sh
```

或者手动：
```bash
idf.py set-target esp32s3
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
│   ├── main.c           # 程序入口
│   ├── app_display.c/h  # 表情管理和动画
│   └── CMakeLists.txt
├── components/
│   ├── gc9a01/          # GC9A01 屏幕驱动
│   └── expressive_eyes/ # 表情渲染引擎 (极低内存)
├── docs/
│   ├── ARCHITECTURE.md
│   └── HARDWARE.md
├── build.sh             # 快速编译脚本
└── CMakeLists.txt
```

## 表情列表

- 😐 NEUTRAL - 中性
- 😊 HAPPY - 开心
- 😢 SAD - 难过
- 😲 SURPRISED - 惊讶
- 😴 SLEEPY - 困倦
- 😠 ANGRY - 生气
- 😑 BORED - 无聊
- 🤩 EXCITED - 兴奋

## 内存占用

仅使用 **~480 字节** 行缓冲，无全屏帧缓冲！
