# PrismPlayer 服务层 API 设计规范

**作者**：邓志鸿  
**日期**：2026-05-30  
**版本**：v1.0  
**模块**：client/service  

---

## 1. 概述

### 1.1 定位

服务层（service）位于 PrismPlayer 六层架构的第 5 层，向下整合 engine（解码渲染）、business/av_sync（音视频同步）、business/network（网络通信），向上为表示层（Flutter/Dart FFI）提供统一 C 风格 API，同时支持二次开发。

```
┌─────────────────────┐
│  表示层 (Flutter)    │  ← Dart FFI 调用 C API
├─────────────────────┤
│  FFI 桥接层          │
├─────────────────────┤
│  ★ 服务层 (service)  │  ← 本设计
├─────────────────────┤
│  业务层 (av_sync)    │
├─────────────────────┤
│  引擎层 (engine)     │
├─────────────────────┤
│  适配层 (adapter)    │
├─────────────────────┤
│  数据持久化层        │
└─────────────────────┘
```

### 1.2 设计原则

- **纯 C 接口**：所有公共函数使用 C 调用约定，确保 FFI 兼容和 ABI 稳定
- **不透明句柄**：通过 `PrismPlayerHandle` 隐藏内部实现，支持多实例
- **错误码返回**：函数返回 `int`（`PRISM_OK` = 0，负数为具体错误）
- **异步回调**：长时间操作（加载、seek）通过 `PrismEventCallback` 通知完成
- **线程安全**：多线程可同时操作不同实例；同一实例的操作需由调用方同步

### 1.3 命名约定

遵循项目代码规范：

| 类别 | 规则 | 示例 |
|------|------|------|
| 结构体 | PascalCase | `PrismConfig`, `PrismMediaInfo` |
| 枚举类型 | PascalCase | `PrismState`, `PrismErrorCode` |
| 枚举值 | UPPER_SNAKE_CASE | `PRISM_STATE_PLAYING`, `PRISM_OK` |
| 函数 | snake_case + 模块前缀 | `prism_player_create` |
| 宏 | UPPER_SNAKE_CASE | `PRISM_VERSION_MAJOR` |

---

## 2. 类型定义

### 2.1 句柄（现有，不变）

```c
typedef struct PrismPlayerInternal* PrismPlayerHandle;
```

### 2.2 播放状态（现有，不变）

```c
typedef enum {
    PRISM_STATE_IDLE = 0,
    PRISM_STATE_LOADING,
    PRISM_STATE_PLAYING,
    PRISM_STATE_PAUSED,
    PRISM_STATE_STOPPED,
    PRISM_STATE_ERROR
} PrismState;
```

**状态机**：

```
IDLE ──[open]──▶ LOADING ──[MEDIA_LOADED]──▶ PAUSED
                    │                            │
                    └──[ERROR]──▶ ERROR          ├──[play]──▶ PLAYING
                                                  │              │
  ERROR  ◀──[解码错误]── (任意)                   │              ├──[pause]──▶ PAUSED
  STOPPED ◀──[stop]────── (任意)                  │              │
  IDLE    ◀──[close]───── (任意)                  │              └──[PLAYBACK_COMPLETED]──▶ STOPPED
                                                  │
                                    PAUSED ◀──[play]── STOPPED
```

### 2.3 事件类型（现有，不变）

```c
typedef enum {
    PRISM_EVENT_MEDIA_LOADED = 0,
    PRISM_EVENT_PLAYBACK_COMPLETED,
    PRISM_EVENT_SEEK_COMPLETED,
    PRISM_EVENT_BUFFERING_START,
    PRISM_EVENT_BUFFERING_END,
    PRISM_EVENT_ERROR
} PrismEventType;

typedef void (*PrismEventCallback)(PrismEventType, const void* data, void* user_data);
```

### 2.4 播放配置（现有基础上新增 enable_audio）

```c
typedef struct {
    int video_output_width;        // 0 = 使用源分辨率
    int video_output_height;       // 0 = 使用源分辨率
    int audio_sample_rate;         // 0 = 使用源采样率
    float default_volume;          // 0.0-1.0，默认 1.0
    bool enable_video;             // false = 纯音频模式
    bool enable_audio;             // true = 同时播放音频（新增）
    const char* log_level;         // "debug"|"info"|"warn"|"error"
} PrismConfig;
```

**默认值**：未提供 config 时（传入 NULL），所有字段使用以下默认值：
- `video_output_width/height`: 0（源分辨率）
- `audio_sample_rate`: 0（源采样率）
- `default_volume`: 1.0
- `enable_video`: true
- `enable_audio`: true
- `log_level`: "info"

### 2.5 错误码（新增）

```c
typedef enum {
    PRISM_OK                   =  0,
    PRISM_ERROR_UNKNOWN        = -1,
    PRISM_ERROR_INVALID_HANDLE = -2,
    PRISM_ERROR_INVALID_PARAM  = -3,
    PRISM_ERROR_NO_MEDIA       = -4,
    PRISM_ERROR_OPEN_FAILED    = -5,
    PRISM_ERROR_SEEK_FAILED    = -6,
    PRISM_ERROR_NOT_SUPPORTED  = -7,
} PrismErrorCode;
```

### 2.6 Seek 模式（新增）

```c
typedef enum {
    PRISM_SEEK_ABSOLUTE = 0,   // 绝对定位到 position_ms
    PRISM_SEEK_RELATIVE = 1,   // 相对当前位置偏移
} PrismSeekMode;
```

### 2.7 媒体信息（新增）

```c
typedef struct {
    int video_width;           // 视频宽度
    int video_height;          // 视频高度
    int64_t duration_ms;       // 总时长（ms），直播流为 -1
    int audio_channels;        // 音频声道数
    int audio_sample_rate;     // 音频采样率
} PrismMediaInfo;
```

---

## 3. API 函数

### 3.1 生命周期

```c
/**
 * @brief 创建播放器实例
 * @param config 播放器配置（可为 NULL，使用默认值）
 * @param callback 事件回调函数（可为 NULL）
 * @param user_data 回调透传的用户数据
 * @return 播放器句柄，失败返回 NULL
 */
PrismPlayerHandle prism_player_create(const PrismConfig* config,
                                      PrismEventCallback callback,
                                      void* user_data);

/**
 * @brief 销毁播放器实例，释放所有资源
 * @param player 播放器句柄（传入 NULL 无操作）
 */
void prism_player_destroy(PrismPlayerHandle player);
```

### 3.2 媒体源

```c
/**
 * @brief 打开媒体源（本地文件路径或网络 URL）
 *        调用后状态变为 PRISM_STATE_LOADING，
 *        加载完成后触发 PRISM_EVENT_MEDIA_LOADED 回调
 * @param player 播放器句柄
 * @param uri 文件路径或网络 URL
 *            - 本地文件: "C:/video.mp4" 或 "file:///path/to/file"
 *            - 网络流: "rtmp://..." "rtsp://..." "http://..." 等
 * @return PRISM_OK 成功开始加载，否则返回错误码
 */
int prism_player_open(PrismPlayerHandle player, const char* uri);

/**
 * @brief 关闭当前媒体，释放解码资源
 *        状态回到 PRISM_STATE_IDLE
 * @param player 播放器句柄
 * @return PRISM_OK
 */
int prism_player_close(PrismPlayerHandle player);
```

### 3.3 播放控制

```c
/**
 * @brief 开始/恢复播放
 *        状态变为 PRISM_STATE_PLAYING
 *        播放完毕后触发 PRISM_EVENT_PLAYBACK_COMPLETED，状态回到 STOPPED
 * @param player 播放器句柄
 * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA
 */
int prism_player_play(PrismPlayerHandle player);

/**
 * @brief 暂停播放，保留解码资源
 *        状态变为 PRISM_STATE_PAUSED
 * @param player 播放器句柄
 * @return PRISM_OK
 */
int prism_player_pause(PrismPlayerHandle player);

/**
 * @brief 停止播放并释放解码资源
 *        状态变为 PRISM_STATE_STOPPED
 *        与 close 的区别：stop 保留媒体信息，可再次 play 从头播放
 * @param player 播放器句柄
 * @return PRISM_OK
 */
int prism_player_stop(PrismPlayerHandle player);

/**
 * @brief 跳转到指定位置
 *        完成后触发 PRISM_EVENT_SEEK_COMPLETED
 * @param player 播放器句柄
 * @param position_ms 目标位置（毫秒）
 * @param mode PRISM_SEEK_ABSOLUTE（绝对）或 PRISM_SEEK_RELATIVE（相对偏移）
 * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA，
 *         不可 seek 返回 PRISM_ERROR_SEEK_FAILED
 */
int prism_player_seek(PrismPlayerHandle player, int64_t position_ms,
                      PrismSeekMode mode);
```

### 3.4 状态查询

```c
/**
 * @brief 获取当前播放状态
 * @param player 播放器句柄
 * @return 当前 PrismState，player 为 NULL 返回 PRISM_STATE_ERROR
 */
PrismState prism_player_get_state(PrismPlayerHandle player);

/**
 * @brief 获取当前播放位置
 * @param player 播放器句柄
 * @return 当前位置（毫秒），无媒体返回 -1
 */
int64_t prism_player_get_position(PrismPlayerHandle player);

/**
 * @brief 获取媒体总时长
 * @param player 播放器句柄
 * @return 总时长（毫秒），直播流/无媒体返回 -1
 */
int64_t prism_player_get_duration(PrismPlayerHandle player);
```

### 3.5 音量控制

```c
/**
 * @brief 设置音量
 * @param player 播放器句柄
 * @param volume 音量 0.0-1.0，超出范围自动钳位
 * @return PRISM_OK
 */
int prism_player_set_volume(PrismPlayerHandle player, float volume);

/**
 * @brief 获取当前音量
 * @param player 播放器句柄
 * @return 音量值 0.0-1.0，无效句柄返回 0.0
 */
float prism_player_get_volume(PrismPlayerHandle player);

/**
 * @brief 设置静音状态
 * @param player 播放器句柄
 * @param mute true=静音 false=取消静音
 * @return PRISM_OK
 */
int prism_player_set_mute(PrismPlayerHandle player, bool mute);

/**
 * @brief 获取静音状态
 * @param player 播放器句柄
 * @return 是否静音，无效句柄返回 false
 */
bool prism_player_get_mute(PrismPlayerHandle player);
```

### 3.6 播放属性

```c
/**
 * @brief 设置播放速度
 * @param player 播放器句柄
 * @param speed 速度倍率，范围 0.5x-2.0x，1.0 为正常速度
 *              超出范围自动钳位
 * @return PRISM_OK
 */
int prism_player_set_playback_speed(PrismPlayerHandle player, float speed);

/**
 * @brief 获取播放速度
 * @param player 播放器句柄
 * @return 当前速度倍率，无效句柄返回 1.0
 */
float prism_player_get_playback_speed(PrismPlayerHandle player);

/**
 * @brief 设置循环播放
 * @param player 播放器句柄
 * @param loop true=单曲循环 false=播完停止
 * @return PRISM_OK
 */
int prism_player_set_loop(PrismPlayerHandle player, bool loop);

/**
 * @brief 获取循环状态
 * @param player 播放器句柄
 * @return 是否循环播放，无效句柄返回 false
 */
bool prism_player_get_loop(PrismPlayerHandle player);
```

### 3.7 视频窗口

```c
/**
 * @brief 设置视频渲染目标窗口
 * @param player 播放器句柄
 * @param native_window 原生窗口句柄（Windows: HWND 转换为 void*）
 *                      传入 NULL 可解除绑定
 * @return PRISM_OK
 */
int prism_player_set_video_window(PrismPlayerHandle player, void* native_window);
```

**平台扩展**：后续 Android/iOS/Linux 平台通过宏区分 `void*` 的实际类型（ANativeWindow*/UIView*/X11 Window）。

### 3.8 媒体信息查询

```c
/**
 * @brief 获取已加载媒体的详细信息
 *        应在收到 PRISM_EVENT_MEDIA_LOADED 之后调用
 * @param player 播放器句柄
 * @param info [out] 输出参数，调用方分配内存
 * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA
 */
int prism_player_get_media_info(PrismPlayerHandle player, PrismMediaInfo* info);
```

### 3.9 诊断工具

```c
/**
 * @brief 获取最后一次失败操作的详细错误码
 * @param player 播放器句柄
 * @return PrismErrorCode
 */
PrismErrorCode prism_player_get_last_error(PrismPlayerHandle player);

/**
 * @brief 获取 SDK 版本字符串
 * @return 版本号，格式 "PrismPlayer x.y.z"
 */
const char* prism_player_get_version(void);
```

---

## 4. API 总览

| 分类 | 函数 | 返回值 | 说明 |
|------|------|--------|------|
| 生命周期 | `prism_player_create` | `PrismPlayerHandle` | 创建实例 |
| | `prism_player_destroy` | `void` | 销毁实例 |
| 媒体源 | `prism_player_open` | `int` | 打开文件/URL |
| | `prism_player_close` | `int` | 关闭媒体 |
| 播放控制 | `prism_player_play` | `int` | 播放 |
| | `prism_player_pause` | `int` | 暂停 |
| | `prism_player_stop` | `int` | 停止 |
| | `prism_player_seek` | `int` | 跳转定位 |
| 状态查询 | `prism_player_get_state` | `PrismState` | 当前状态 |
| | `prism_player_get_position` | `int64_t` | 播放位置(ms) |
| | `prism_player_get_duration` | `int64_t` | 总时长(ms) |
| 音量 | `prism_player_set_volume` | `int` | 设置音量 |
| | `prism_player_get_volume` | `float` | 获取音量 |
| | `prism_player_set_mute` | `int` | 设置静音 |
| | `prism_player_get_mute` | `bool` | 获取静音 |
| 播放属性 | `prism_player_set_playback_speed` | `int` | 设置速度 |
| | `prism_player_get_playback_speed` | `float` | 获取速度 |
| | `prism_player_set_loop` | `int` | 设置循环 |
| | `prism_player_get_loop` | `bool` | 获取循环 |
| 视频 | `prism_player_set_video_window` | `int` | 绑定渲染窗口 |
| 信息 | `prism_player_get_media_info` | `int` | 获取媒体信息 |
| 诊断 | `prism_player_get_last_error` | `PrismErrorCode` | 错误码 |
| | `prism_player_get_version` | `const char*` | 版本号 |

共 **22 个函数**。

---

## 5. 调用流程示例

### 5.1 基本播放流程

```c
// 1. 创建播放器
PrismConfig config = {
    .default_volume = 1.0f,
    .enable_video = true,
    .enable_audio = true,
    .log_level = "info"
};
PrismPlayerHandle player = prism_player_create(&config, on_event, NULL);

// 2. 打开文件（异步）
prism_player_open(player, "C:/videos/demo.mp4");

// 3. 等待 PRISM_EVENT_MEDIA_LOADED 回调...

// 4. 获取媒体信息
PrismMediaInfo info;
prism_player_get_media_info(player, &info);

// 5. 设置视频窗口
prism_player_set_video_window(player, hwnd);

// 6. 播放
prism_player_play(player);

// 7. 定期查询进度
int64_t pos = prism_player_get_position(player);
int64_t dur = prism_player_get_duration(player);

// 8. 播放完毕或用户停止
prism_player_stop(player);
prism_player_close(player);
prism_player_destroy(player);
```

### 5.2 事件回调实现

```c
void on_event(PrismEventType type, const void* data, void* user_data) {
    switch (type) {
    case PRISM_EVENT_MEDIA_LOADED:
        // 媒体加载完毕，可以获取 duration 等信息
        break;
    case PRISM_EVENT_PLAYBACK_COMPLETED:
        // 播放结束
        break;
    case PRISM_EVENT_SEEK_COMPLETED:
        // Seek 完成
        break;
    case PRISM_EVENT_BUFFERING_START:
        // 开始缓冲（网络流场景）
        break;
    case PRISM_EVENT_BUFFERING_END:
        // 缓冲完成
        break;
    case PRISM_EVENT_ERROR:
        // 发生错误，data 指向 PrismErrorCode
        break;
    }
}
```

---

## 6. 内部实现接口

服务层内部使用 C++ 实现，通过 `PlayerImpl` 类整合下层模块：

```cpp
// client/service/Impl/PlayerImpl.h
namespace Prism::Service {

class PlayerImpl {
public:
    explicit PlayerImpl(const PrismConfig& config,
                        PrismEventCallback callback,
                        void* user_data);
    ~PlayerImpl();

    int open(const char* uri);
    int close();
    int play();
    int pause();
    int stop();
    int seek(int64_t position_ms, int seek_mode);

    PrismState get_state() const;
    int64_t get_position() const;
    int64_t get_duration() const;
    void    get_media_info(PrismMediaInfo* info) const;
    PrismErrorCode get_last_error() const;

    int   set_volume(float volume);
    float get_volume() const;
    int   set_mute(bool mute);
    bool  get_mute() const;
    int   set_playback_speed(float speed);
    float get_playback_speed() const;
    int   set_loop(bool loop);
    bool  get_loop() const;
    int   set_video_window(void* native_window);

private:
    // 拥有的下层模块
    std::unique_ptr<Prism::Engine::AudioEngine> audio_engine_;
    std::unique_ptr<Prism::Engine::VideoEngine> video_engine_;
    // AV 同步模块（待实现）
    // 网络模块（待实现）

    PrismConfig config_;
    PrismEventCallback callback_;
    void* user_data_;
    PrismState state_;
    PrismErrorCode last_error_;
    // ...
};

} // namespace Prism::Service
```

---

## 7. 文件清单

| 文件 | 作用 | 状态 |
|------|------|------|
| `client/service/include/Types.h` | 公共类型定义（句柄、枚举、结构体） | 已有基础，需扩展 |
| `client/service/include/Player.h` | 公共 API 函数声明（C 风格） | 待实现 |
| `client/service/Impl/PlayerImpl.h` | 内部实现类声明（C++） | 待实现 |
| `client/service/src/Player.cpp` | API 函数实现 + PlayerImpl 实现 | 待实现 |

---

## 8. 后续扩展预留

以下功能本版本暂不包含，但 API 设计已预留扩展空间：

- **字幕轨选择**：可增加 `prism_player_set_subtitle_track(int index)` 和 `PRISM_EVENT_SUBTITLE_DATA`
- **多音轨切换**：可增加 `prism_player_set_audio_track(int index)`
- **音效均衡器**：可增加 `prism_player_set_equalizer(const PrismEqualizer* eq)`
- **截图**：可增加 `prism_player_take_snapshot(const char* save_path)`
- **画中画**：可增加 `prism_player_set_pip(bool enable)`
- **投屏/远程播放**：利用 business/network 模块实现
