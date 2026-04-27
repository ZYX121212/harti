# 软件架构设计

## 目录结构

```
harti/
├── main/
│   ├── main.c                 # 入口
│   ├── app_display.c          # 显示/表情管理
│   ├── app_sensors.c          # 传感器采集
│   ├── app_behavior.c         # 行为状态机
│   └── app_audio.c            # 音频/AI对话
├── components/
│   ├── expressive_eyes/       # 表情渲染核心 (espp)
│   ├── gc9a01/                # 屏幕驱动
│   └── ...
└── README.md
```

---

## 核心模块设计

### 1. app_display - 表情管理

职责：
- 初始化 GC9A01 屏幕
- 管理 expressive_eyes 状态
- 提供接口：`set_emotion(EMOTION_HAPPY)` 等

```c
typedef enum {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_SAD,
    EMOTION_SURPRISED,
    EMOTION_SLEEPY,
    EMOTION_ANGRY,
    EMOTION_BORED,
    EMOTION_EXCITED,
    EMOTION_CONFUSED,
    EMOTION_CONTENT
} emotion_t;
```

### 2. app_sensors - 传感器 Hub

职责：
- 定时采集各传感器
- 检测事件（摇晃、触摸、巨响等）
- 通过队列发送事件给行为模块

```c
typedef enum {
    EVT_TOUCH_HEAD,       // 触摸头部
    EVT_SHAKE,            // 摇晃机身
    EVT_LOUD_NOISE,       // 大声响
    EVT_LIGHT_CHANGE,     // 光线变化
    EVT_WAKE_WORD,        // 唤醒词
    EVT_NONE
} sensor_event_t;
```

### 3. app_behavior - 状态机

职责：
- 接收传感器事件
- 维护当前情绪/状态
- 决策下一个表情/动作
- 处理超时（无聊、休眠）

状态机：
```
[ IDLE ] --(触摸)--> [ HAPPY ]
[ IDLE ] --(摇晃)--> [ SURPRISED ]
[ IDLE ] --(10s无操作)--> [ BORED ]
[ BORED ] --(30s)--> [ SLEEPY ]
```

### 4. app_audio - 音频与AI (可选)

职责：
- 离线唤醒词检测
- 录音 + 上传云端
- 播放TTS

---

## 数据流向

```
传感器 (I2C/I2S/Touch)
    |
    v
app_sensors (事件检测)
    |
    +---> Queue ---> app_behavior (决策)
                        |
                        +---> app_display (更新表情)
                        |
                        +---> app_audio (触发对话)
```

