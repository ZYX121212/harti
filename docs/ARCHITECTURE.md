# 软件架构设计

## 目录结构

```
harti/
├── main/
│   ├── main.c              # 入口，创建 FreeRTOS 任务
│   ├── harti_config.h      # 统一配置（常量/阈值/类型）
│   ├── app_sensors.c/h     # 传感器事件检测
│   ├── app_behavior.c/h    # 行为状态机 + 关系管理
│   ├── app_input.c/h       # 物理输入（按键/触摸）
│   └── app_ble.c/h         # BLE 碰一碰通信
├── components/
│   ├── face_system/        # 表情渲染引擎
│   │   ├── face_api.h      # 公共 API（应用层唯一入口）
│   │   ├── face_model.h/c  # 数据模型 + 表情预设
│   │   ├── face_animator.h/c # 动画引擎（LVGL tween）
│   │   ├── face_renderer.h/c # 渲染器（扫描线合成）
│   │   ├── face_palette.h/c  # 调色板定义
│   │   ├── face_common.h     # 内部工具函数
│   │   └── sprites/          # sprite 资产
│   │       ├── sprite_registry.h/c  # [唯一真相源] 管理所有 sprite
│   │       ├── sprite_vector.c/h
│   │       ├── sprite_lineart.c/h
│   │       ├── sprite_classic.c/h
│   │       ├── sprite_cat.c/h
│   │       ├── sprite_pixel.c/h
│   │       └── sprite_robot.c/h
│   ├── gc9a01/             # GC9A01 LCD SPI 驱动
│   ├── harti_imu/          # IMU 驱动 (MPU6050, I2C)
│   └── harti_temp/         # 温度传感器驱动 (NTC + ADC)
```

---

## 图层与依赖关系

```
应用编排层 (main/)
    │
    ├── app_behavior ──→ face_api.h ──→ face_animator / face_renderer
    │                                       (不暴露 sprite 细节)
    │
    ├── app_input ──→ sprite_registry.h ──→ 各 sprite
    │                 (只 registry 知道全部 sprite)
    │
    ├── app_sensors ──→ 驱动层 (harti_imu, harti_temp)
    │
    └── app_ble ──→ BLE 协议栈
```

核心原则：
- **应用层不直接引用 sprite 头文件**：所有 sprite 访问通过 `sprite_registry`
- **face_api.h 是 face_system 的唯一公共入口**：应用层不直接调用 `animator_*` / `renderer_*`
- **harti_config.h 集中管理常量**：不在各模块重复定义

---

## 核心模块

### 1. face_system — 表情渲染

**职责**: 管理表情状态、动画过渡、渲染输出

**已实现**:
- 扫描线渲染（8 层合成：脸部、腮红、嘴、左右眼、左右眉、装饰）
- 参数化面部模型（脸型、眉、眼、嘴、装饰 5 类 component）
- 13 种预设表情 + 6 种 sprite 风格
- LVGL tween 驱动的平滑过渡（5 种缓动路径）
- 微动画（眨眼、呼吸、微扫视）

**sprite registry 模式**:
```c
// 唯一真相源: sprite_registry.c
static const sprite_set_t *const SPRITES[] = {
    &SPRITE_VECTOR, &SPRITE_LINEART, &SPRITE_CLASSIC,
    &SPRITE_CAT, &SPRITE_PIXEL, &SPRITE_ROBOT,
};

// 加新 sprite：只需创建 sprite_xxx.c/h + 在上方数组添加一行
```

```c
// 表情枚举
typedef enum {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY, EMOTION_SAD, EMOTION_SURPRISED, EMOTION_SLEEPY,
    EMOTION_ANGRY, EMOTION_BORED, EMOTION_EXCITED, EMOTION_CONFUSED,
    EMOTION_CONTENT, EMOTION_COLD, EMOTION_WARM, EMOTION_HEART_EYES,
    EMOTION_COUNT
} emotion_t;
```

### 2. app_sensors — 传感器事件检测

**职责**: 采集传感器数据，检测交互事件，通过队列发送给行为模块

```c
typedef enum {
    EVT_NONE = 0,
    EVT_SHAKE, EVT_TAP, EVT_FLIP,
    EVT_WARM_UP, EVT_COLD_DOWN,
    EVT_BLE_MEET, EVT_BLE_FRIEND,
    EVT_TWIST, EVT_TILT,
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
    ├──> face_set_expression() ──> face_system 渲染
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
| `display_task` | High (5) | 4KB | 20fps | 动画 + 渲染 |
| `sensor_task` | Medium (3) | 2KB | 100Hz | IMU/触摸/温度采样 |
| `behavior_task` | Medium (3) | 2KB | 事件驱动 | 状态机决策 |
| `input_task` | Medium (3) | 2KB | 30ms | 按键检测 + sprite 轮换 |
| `ble_task` | Low (1) | 3KB | 事件驱动 | BLE 扫描/连接 |

队列:
- `sensor_queue`: 10 条 `sensor_event_msg_t`，sensor_task → behavior_task

---

## 渲染引擎优化

| 优化项 | 方法 | 效果 |
|--------|------|------|
| 背景渐变 | LUT + fast_isqrt 代替 sqrtf | 消除 57,600 次/帧浮点运算 |
| 颜色混合 | 8.8 定点数代替浮点乘法 | 每次混合省 3 次浮点乘 |
| 渲染范围 | 边界框裁剪 + 扫描行跳过 | 减少约 50% 无效像素遍历 |
| 边缘效果 | 1.5px 抗锯齿 + 双高光 | 视觉品质提升 |
| 循环不变量 | 虹膜/瞳孔/眼睑位置提到循环外 | 减少重复计算 |
