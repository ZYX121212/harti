# Harti 传感器管线 + 表情特效 设计规格

**日期**: 2026-05-17
**状态**: 已确认

---

## 范围

本规格涵盖并行推进的两条线：

1. **传感器→行为→BLE 管线**: app_sensors, app_behavior, app_ble 三个新模块
2. **表情特效层**: 5 种新表情 + 心形眼 + 碰一碰特效 (app_effects)

---

## 新增模块

### app_sensors

- I2C 总线初始化 (master bus)
- 驱动 IMU (harti_imu, 100Hz), 触摸 (ESP32-S3 电容触摸 1通道, 50Hz), 温度 (harti_temp, 1Hz)
- 事件检测逻辑: 摇晃(accel > 2.5g 持续 > 300ms), 轻拍(Z 脉冲 < 100ms), 翻转(Z 反转 > 500ms), 捂热(升温 > 2°C/5s), 触摸(> 50ms 去抖)
- 通过 FreeRTOS Queue 发送 `sensor_event_t` 给 behavior_task

### app_behavior

- 行为状态机: IDLE → 各情绪 → 超时 BORED(10s) → SLEEPY(30s)
- 空闲计时器: 10s 无互动转 BORED，30s 转 SLEEPY
- NVS 关系存储: friend_00~0f, 每条 20 字节
- 五级关系升级逻辑

### app_ble

- 首版 stub 实现，仅提供函数框架和事件队列
- BLE 广播 + 扫描框架预留
- 后续迭代补充完整配对和数据交换

### app_effects

- 独立渲染层，叠加在眼睛之上
- 特效类型: 小星星(EFFECT_STAR), 心形粒子(EFFECT_HEART_PARTICLE), 彩虹弧(EFFECT_RAINBOW), 金光(EFFECT_GOLDEN)
- 生命周期: 淡入(0.3s) → 停留(1.5s) → 淡出(0.5s) → 释放
- 更高级特效可中断低级特效

---

## 修改模块

### app_display

- 新增 5 个 emotion_t 枚举值: EMOTION_CONFUSED, EMOTION_CONTENT, EMOTION_COLD, EMOTION_WARM, EMOTION_HEART_EYES
- 新增心形眼模式标志

### expressive_eyes

- 心形虹膜: 16x16 查表，存储心形轮廓点，render_eye 中当 heart_mode=true 时用查表替代圆形虹膜
- 新增 EYE_STATE_HEART_EYES 预设

### main.c

- 重构为 4 任务 FreeRTOS 架构
- I2C 总线初始化
- 队列创建

---

## 数据结构

```c
// 传感器事件
typedef enum {
    EVT_NONE = 0,
    EVT_TOUCH_HEAD, EVT_TOUCH_RELEASE,
    EVT_SHAKE, EVT_TAP, EVT_FLIP,
    EVT_WARM_UP, EVT_COLD_DOWN,
    EVT_BLE_MEET, EVT_BLE_FRIEND,
} sensor_event_t;

// 特效类型
typedef enum {
    EFFECT_NONE = 0,
    EFFECT_STAR,
    EFFECT_HEART_PARTICLE,
    EFFECT_RAINBOW,
    EFFECT_GOLDEN,
} effect_type_t;

// 关系记录
typedef struct {
    uint8_t  ble_id[6];
    uint8_t  level;
    uint8_t  meet_count;
    uint32_t last_seen;
    char     nickname[8];
} friend_record_t;
```

---

## FreeRTOS 任务

| 任务 | 优先级 | 栈 | 频率 |
|------|--------|-----|------|
| display_task | 5 | 4KB | 60fps 定时 |
| sensor_task | 3 | 2KB | 100Hz 定时 |
| behavior_task | 3 | 2KB | 事件驱动 |
| ble_task | 1 | 3KB | 事件驱动 |

队列: sensor_queue (10 slot), ble_queue (5 slot)

---

## 心形查表

16x16 二值表，约 32 字节（2 位每像素或直接存坐标列表 ~40 个点 = 80 字节 int8 坐标对）。用坐标列表方式，存储相对于虹膜中心的偏移坐标，渲染时遍历列表检查距离。
