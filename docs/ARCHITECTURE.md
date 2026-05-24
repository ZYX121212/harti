# 软件架构设计

## 目录结构

```
harti/
├── main/
│   ├── main.c              # 入口，创建 FreeRTOS 任务
│   ├── app_display.c/h     # 表情渲染 + 微动画
│   ├── app_sensors.c/h     # 传感器事件检测
│   ├── app_behavior.c/h    # 行为状态机 + 关系管理
│   └── app_ble.c/h         # BLE 碰一碰通信
├── components/
│   ├── expressive_eyes/    # 眼睛渲染引擎 (扫描线方式，~480 字节内存)
│   ├── gc9a01/             # GC9A01 LCD SPI 驱动
│   ├── harti_imu/          # IMU 驱动 (MPU6050, I2C)
│   └── harti_temp/         # 温度传感器驱动 (NTC + ADC)
```

---

## 核心模块

### 1. app_display — 表情渲染

**职责**: 管理表情状态、微动画、渲染输出

**已实现**:
- 扫描线渲染 (背景渐变、眼睛、虹膜、瞳孔、高光、边缘抗锯齿)
- 装饰层 (腮红、眼泪、星星)
- 13 种预设表情 + 双色主题 (白/黑)
- 平滑过渡 (ease-out-cubic)
- 微动画 (眨眼、呼吸、微扫视)

**待新增**:
- 碰一碰特效动画 (心心、彩虹、金光)
- 温度表盘模式
- 时间感知 (早中晚不同默认表情)

```c
// 表情枚举 (扩展后)
typedef enum {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_SAD,
    EMOTION_SURPRISED,
    EMOTION_SLEEPY,
    EMOTION_ANGRY,
    EMOTION_BORED,
    EMOTION_EXCITED,
    EMOTION_CONFUSED,     // 新增: 摇晃后
    EMOTION_CONTENT,      // 新增: 持续摸头
    EMOTION_COLD,         // 新增: 低温
    EMOTION_WARM,         // 新增: 被捂热
    EMOTION_HEART_EYES,   // 新增: 挚友碰面
    EMOTION_COUNT
} emotion_t;
```

### 2. app_sensors — 传感器事件检测

**职责**: 采集传感器数据，检测交互事件，通过队列发送给行为模块

```c
// 传感器事件
typedef enum {
    EVT_TOUCH_HEAD,      // 摸头
    EVT_TOUCH_RELEASE,   // 手离开
    EVT_SHAKE,           // 摇晃
    EVT_TAP,             // 轻拍
    EVT_FLIP,            // 翻转
    EVT_WARM_UP,         // 温度上升 (被捂热)
    EVT_COLD_DOWN,       // 温度骤降
    EVT_BLE_MEET,        // 碰一碰遇到新朋友
    EVT_BLE_FRIEND,      // 碰一碰遇到老朋友
    EVT_NONE
} sensor_event_t;
```

**传感器配置**:
- IMU (MPU6050): 100Hz 采样，I2C 通信
- 触摸: ESP32-S3 内置电容触摸，50ms 去抖
- 温度 (NTC): 1Hz 采样，ADC 读取

### 3. app_behavior — 行为状态机

**职责**: 接收传感器事件，决策表情/动作，管理碰一碰关系

**状态机**:
```
IDLE ──[摸头]──> HAPPY ──[持续摸]──> CONTENT ──[放手]──> IDLE
IDLE ──[摇晃]──> SURPRISED ──[停]──> CONFUSED ──[超时]──> IDLE
IDLE ──[轻拍]──> BLINK ──[连拍]──> SAD ──[超时]──> IDLE
IDLE ──[捂热]──> WARM ──[放手]──> IDLE
IDLE ──[10s无互动]──> BORED ──[30s]──> SLEEPY
IDLE ──[碰一碰]──> MEET_REACTION (根据关系等级不同)
IDLE ──[温度<15°C]──> COLD ──[温度回升]──> IDLE
```

**关系管理**:
- 最多 16 个朋友，存 NVS
- 五级关系: 陌生人 → 点头之交 → 朋友 → 好友 → 挚友
- 关系衰减: 7 天不碰面触发"好久不见"反应

### 4. app_ble — BLE 碰一碰通信

**职责**: BLE 扫描、广播、配对、数据交换

- 降低发射功率实现 ~10cm 感知范围
- 专用 Service UUID，自动发现其他 Harti
- 交换设备 ID + 关系等级 + 见面次数
- 碰面后写入 NVS，触发等级升级检查

---

## 数据流

```
传感器 (I2C/Touch/ADC)
    │
    v
app_sensors (事件检测, 100Hz)
    │
    ├──> Queue (sensor_event_msg_t)
    │
    v
app_behavior (状态机决策)
    │
    ├──> app_display (set_emotion)
    │
    ├──> app_ble (碰一碰事件)
    │       │
    │       v
    │    NVS (关系存储)
    │
    └──> 定时器 (超时转 BORED/SLEEPY)
```

---

## FreeRTOS 任务结构

| 任务 | 优先级 | 栈大小 | 频率 | 职责 |
|------|--------|--------|------|------|
| `display_task` | High (5) | 4KB | 60fps | 渲染 + 微动画 |
| `sensor_task` | Medium (3) | 2KB | 100Hz | IMU/触摸/温度采样 |
| `behavior_task` | Medium (3) | 2KB | 事件驱动 | 状态机决策 |
| `ble_task` | Low (1) | 3KB | 事件驱动 | BLE 扫描/连接 |

队列:
- `sensor_queue`: 10 条 `sensor_event_msg_t`，sensor_task → behavior_task

---

## 渲染引擎优化

expressive_eyes 组件的关键优化:

| 优化项 | 方法 | 效果 |
|--------|------|------|
| 背景渐变 | LUT + fast_isqrt 代替 sqrtf | 消除 57,600 次/帧浮点运算 |
| 颜色混合 | 8.8 定点数代替浮点乘法 | 每次混合省 3 次浮点乘 |
| 渲染范围 | 边界框裁剪 + 扫描行跳过 | 减少约 50% 无效像素遍历 |
| 边缘效果 | 1.5px 抗锯齿 + 双高光 | 视觉品质提升 |
| 循环不变量 | 虹膜/瞳孔/眼睑位置提到循环外 | 减少重复计算 |
