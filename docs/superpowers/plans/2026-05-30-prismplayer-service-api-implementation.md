# PrismPlayer 服务层 API 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 PrismPlayer 服务层完整 API（22 个 C 函数 + PlayerImpl C++ 实现类）

**Architecture:** 纯 C 公共接口（Player.h）通过不透明句柄隐藏 PlayerImpl C++ 实现。PlayerImpl 持有 AudioEngine/VideoEngine 实例（通过工厂创建），管理播放状态机，协调 A/V 同步。

**Tech Stack:** C++17, CMake + Ninja + Clang, engine layer (ffmpeg/Vulkan/SoundTouch), spdlog

**Spec:** `docs/superpowers/specs/2026-05-30-prismplayer-service-api-design.md`

---

### Task 1: 修复 CMakeLists.txt 链接配置

**Files:**
- Modify: `client/service/CmakeLists.txt:1-21`

- [ ] **Step 1: 修复 target_link_directories → target_link_libraries 并添加 engine 依赖**

当前 `client/service/CmakeLists.txt` 使用了 `target_link_directories`（仅添加库搜索路径，不实际链接），需改为 `target_link_libraries`。同时服务层需要直接使用 engine 的 AudioEngine/VideoEngine 类型，需添加 `client-engine` 依赖。

将文件内容替换为：

```cmake
project(client-service)

message(STATUS "${PROJECT_NAME} is building")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)

if(SRC)
    add_library(${PROJECT_NAME} SHARED ${SRC})
    target_include_directories(${PROJECT_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Impl
    )

    target_link_libraries(${PROJECT_NAME}
    PRIVATE
        client-engine
        client-av_sync
        client-network
    )
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
```

- [ ] **Step 2: 验证 CMake 配置通过**

```bash
cmake --preset windows-x64-Debug
```

Expected: 配置成功，无错误。

- [ ] **Step 3: Commit**

```bash
git add client/service/CmakeLists.txt
git commit -m "fix(service): repair CMake link config - use target_link_libraries and add engine dependency"
```

---

### Task 2: 扩展 Types.h 类型定义

**Files:**
- Modify: `client/service/include/Types.h:1-30`

- [ ] **Step 1: 添加新类型定义**

将 `client/service/include/Types.h` 内容替换为：

```c
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 版本信息 ========== */
#define PRISM_VERSION_MAJOR 0
#define PRISM_VERSION_MINOR 0
#define PRISM_VERSION_PATCH 1

/* ========== 不透明句柄 ========== */
typedef struct PrismPlayerInternal* PrismPlayerHandle;

/* ========== 播放状态 ========== */
typedef enum {
    PRISM_STATE_IDLE = 0,
    PRISM_STATE_LOADING,
    PRISM_STATE_PLAYING,
    PRISM_STATE_PAUSED,
    PRISM_STATE_STOPPED,
    PRISM_STATE_ERROR
} PrismState;

/* ========== 事件类型 ========== */
typedef enum {
    PRISM_EVENT_MEDIA_LOADED = 0,
    PRISM_EVENT_PLAYBACK_COMPLETED,
    PRISM_EVENT_SEEK_COMPLETED,
    PRISM_EVENT_BUFFERING_START,
    PRISM_EVENT_BUFFERING_END,
    PRISM_EVENT_ERROR
} PrismEventType;

typedef void (*PrismEventCallback)(PrismEventType type, const void* data, void* user_data);

/* ========== 错误码 ========== */
typedef enum {
    PRISM_OK                   =  0,
    PRISM_ERROR_UNKNOWN        = -1,
    PRISM_ERROR_INVALID_HANDLE = -2,
    PRISM_ERROR_INVALID_PARAM  = -3,
    PRISM_ERROR_NO_MEDIA       = -4,
    PRISM_ERROR_OPEN_FAILED    = -5,
    PRISM_ERROR_SEEK_FAILED    = -6,
    PRISM_ERROR_NOT_SUPPORTED  = -7
} PrismErrorCode;

/* ========== Seek 模式 ========== */
typedef enum {
    PRISM_SEEK_ABSOLUTE = 0,
    PRISM_SEEK_RELATIVE = 1
} PrismSeekMode;

/* ========== 播放配置 ========== */
typedef struct {
    int video_output_width;
    int video_output_height;
    int audio_sample_rate;
    float default_volume;
    bool enable_video;
    bool enable_audio;
    const char* log_level;
} PrismConfig;

/* ========== 媒体信息 ========== */
typedef struct {
    int video_width;
    int video_height;
    int64_t duration_ms;
    int audio_channels;
    int audio_sample_rate;
} PrismMediaInfo;

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: Commit**

```bash
git add client/service/include/Types.h
git commit -m "feat(service): extend Types.h with error codes, seek mode, media info, and config update"
```

---

### Task 3: 编写 Player.h 公共 API 声明

**Files:**
- Modify: `client/service/include/Player.h:1` (当前为空)

- [ ] **Step 1: 写入完整 C API 声明**

将 `client/service/include/Player.h` 内容替换为：

```c
#pragma once

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 生命周期 ========== */

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

/* ========== 媒体源 ========== */

/**
 * @brief 打开媒体源（本地文件路径或网络 URL）
 *        调用后状态变为 PRISM_STATE_LOADING，
 *        加载完成后触发 PRISM_EVENT_MEDIA_LOADED 回调
 * @param player 播放器句柄
 * @param uri 文件路径或网络 URL
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

/* ========== 播放控制 ========== */

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

/* ========== 状态查询 ========== */

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

/* ========== 音量控制 ========== */

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

/* ========== 播放属性 ========== */

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

/* ========== 视频窗口 ========== */

/**
 * @brief 设置视频渲染目标窗口
 * @param player 播放器句柄
 * @param native_window 原生窗口句柄（Windows: HWND 转换为 void*）
 *                      传入 NULL 可解除绑定
 * @return PRISM_OK
 */
int prism_player_set_video_window(PrismPlayerHandle player, void* native_window);

/* ========== 媒体信息 ========== */

/**
 * @brief 获取已加载媒体的详细信息
 *        应在收到 PRISM_EVENT_MEDIA_LOADED 之后调用
 * @param player 播放器句柄
 * @param info [out] 输出参数，调用方分配内存
 * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA
 */
int prism_player_get_media_info(PrismPlayerHandle player, PrismMediaInfo* info);

/* ========== 诊断工具 ========== */

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

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: Commit**

```bash
git add client/service/include/Player.h
git commit -m "feat(service): add Player.h with complete C API declarations (22 functions)"
```

---

### Task 4: 编写 PlayerImpl.h 内部实现类

**Files:**
- Modify: `client/service/Impl/PlayerImpl.h:1` (当前为空)

- [ ] **Step 1: 写入 PlayerImpl 类声明**

将 `client/service/Impl/PlayerImpl.h` 内容替换为：

```cpp
#pragma once

#include "Player.h"
#include "AudioEngine.h"
#include "AudioEngineFactory.h"
#include "AudioEngineWasapiSharedFactory.h"
#include "VideoEngine.h"
#include "VideoEngineFactory.h"
#include "VideoEngineVulkanFactory.h"

#include <memory>
#include <atomic>
#include <string>

namespace Prism::Service {

struct PrismPlayerInternal {
    explicit PrismPlayerInternal(const PrismConfig& cfg,
                                 PrismEventCallback cb,
                                 void* ud);
    ~PrismPlayerInternal();

    void fire_event(PrismEventType type, const void* data = nullptr) const;

    // 引擎工厂
    Prism::Engine::AudioEngineWasapiSharedFactory audio_factory_;
    Prism::Engine::VideoEngineVulkanFactory         video_factory_;

    // 引擎实例（延迟创建：首次 open 时 init）
    std::unique_ptr<Prism::Engine::AudioEngine> audio_engine_;
    std::unique_ptr<Prism::Engine::VideoEngine> video_engine_;

    // 媒体信息缓存
    PrismMediaInfo media_info_;

    // 配置与回调
    PrismConfig        config_;
    PrismEventCallback callback_;
    void*              user_data_;

    // 播放状态
    std::atomic<PrismState> state_{PRISM_STATE_IDLE};

    // 音频属性
    std::atomic<float> volume_{1.0f};
    std::atomic<bool>  mute_{false};
    float              volume_before_mute_{1.0f};

    // 播放属性
    std::atomic<float> speed_{1.0f};
    std::atomic<bool>  loop_{false};

    // 视频窗口
    void* video_window_{nullptr};

    // 错误诊断
    std::atomic<PrismErrorCode> last_error_{PRISM_OK};

    // 媒体 URI（用于重新 open）
    std::string media_uri_;

    // 是否已初始化引擎
    bool engines_initialized_{false};
};

} // namespace Prism::Service
```

- [ ] **Step 2: Commit**

```bash
git add client/service/Impl/PlayerImpl.h
git commit -m "feat(service): add PlayerImpl.h - internal implementation class skeleton"
```

---

### Task 5: 编写 Player.cpp 完整实现

**Files:**
- Modify: `client/service/src/Player.cpp:1` (当前为空)

- [ ] **Step 1: 写入完整实现**

将 `client/service/src/Player.cpp` 内容替换为：

```cpp
#include "PlayerImpl.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <spdlog/spdlog.h>

namespace Prism::Service {

/* ========== PrismPlayerInternal 实现 ========== */

static PrismConfig default_config() {
    PrismConfig c{};
    c.video_output_width  = 0;
    c.video_output_height = 0;
    c.audio_sample_rate   = 0;
    c.default_volume      = 1.0f;
    c.enable_video        = true;
    c.enable_audio        = true;
    c.log_level           = "info";
    return c;
}

PrismPlayerInternal::PrismPlayerInternal(const PrismConfig& cfg,
                                         PrismEventCallback cb,
                                         void* ud)
    : config_(cfg)
    , callback_(cb)
    , user_data_(ud)
{
    volume_.store(config_.default_volume);
    spdlog::set_level(spdlog::level::from_str(config_.log_level ? config_.log_level : "info"));
    spdlog::info("[PrismPlayer] instance created");
}

PrismPlayerInternal::~PrismPlayerInternal()
{
    if (engines_initialized_) {
        if (audio_engine_) audio_engine_->close();
        if (video_engine_) video_engine_->close();
    }
    spdlog::info("[PrismPlayer] instance destroyed");
}

void PrismPlayerInternal::fire_event(PrismEventType type, const void* data) const
{
    if (callback_) {
        callback_(type, data, user_data_);
    }
}

/* ========== 引擎初始化辅助 ========== */

static bool init_engines(PrismPlayerInternal* p)
{
    if (p->engines_initialized_) return true;

    if (!p->audio_engine_) {
        p->audio_engine_ = p->audio_factory_.create_audio_engine();
        if (!p->audio_engine_ || !p->audio_engine_->init()) {
            spdlog::error("[PrismPlayer] failed to init audio engine");
            p->last_error_.store(PRISM_ERROR_UNKNOWN);
            return false;
        }
    }

    if (!p->video_engine_ && p->config_.enable_video) {
        p->video_engine_ = p->video_factory_.create_audio_engine(); // VideoEngineFactory bug: method named create_audio_engine
        if (!p->video_engine_ || !p->video_engine_->init()) {
            spdlog::error("[PrismPlayer] failed to init video engine");
            p->last_error_.store(PRISM_ERROR_UNKNOWN);
            return false;
        }
    }

    p->engines_initialized_ = true;
    return true;
}

/* ========== 值钳位辅助 ========== */

static float clamp(float val, float lo, float hi)
{
    return std::max(lo, std::min(hi, val));
}

} // namespace Prism::Service

/* ========== C API 实现 ========== */

extern "C" {

PrismPlayerHandle prism_player_create(const PrismConfig* config,
                                      PrismEventCallback callback,
                                      void* user_data)
{
    Prism::Service::PrismConfig cfg = config ? *config : Prism::Service::default_config();

    auto* p = new (std::nothrow) Prism::Service::PrismPlayerInternal(cfg, callback, user_data);
    if (!p) return nullptr;

    return reinterpret_cast<PrismPlayerHandle>(p);
}

void prism_player_destroy(PrismPlayerHandle player)
{
    if (!player) return;
    delete reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);
}

int prism_player_open(PrismPlayerHandle player, const char* uri)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    if (!uri)  return PRISM_ERROR_INVALID_PARAM;

    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);

    p->state_.store(PRISM_STATE_LOADING);
    p->media_uri_ = uri;

    if (!Prism::Service::init_engines(p)) {
        p->state_.store(PRISM_STATE_ERROR);
        return PRISM_ERROR_OPEN_FAILED;
    }

    // TODO: 实际媒体加载由 engine 层完成，当前 engine 实现为空壳
    // 加载完成后应调用 p->fire_event(PRISM_EVENT_MEDIA_LOADED)
    spdlog::info("[PrismPlayer] open: {}", uri);

    // 模拟加载完成（engine 实现后移除此行）
    p->state_.store(PRISM_STATE_PAUSED);
    p->fire_event(PRISM_EVENT_MEDIA_LOADED);

    return PRISM_OK;
}

int prism_player_close(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->audio_engine_) p->audio_engine_->close();
    if (p->video_engine_) p->video_engine_->close();

    p->engines_initialized_ = false;
    p->media_uri_.clear();
    p->state_.store(PRISM_STATE_IDLE);

    spdlog::info("[PrismPlayer] closed");
    return PRISM_OK;
}

int prism_player_play(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);
    PrismState s = p->state_.load();

    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        p->last_error_.store(PRISM_ERROR_NO_MEDIA);
        return PRISM_ERROR_NO_MEDIA;
    }

    if (s == PRISM_STATE_PLAYING) return PRISM_OK;

    if (!Prism::Service::init_engines(p)) {
        p->state_.store(PRISM_STATE_ERROR);
        return PRISM_ERROR_UNKNOWN;
    }

    if (p->audio_engine_) p->audio_engine_->play();
    if (p->video_engine_) p->video_engine_->play();

    p->state_.store(PRISM_STATE_PLAYING);
    spdlog::info("[PrismPlayer] playing");
    return PRISM_OK;
}

int prism_player_pause(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->state_.load() != PRISM_STATE_PLAYING) return PRISM_OK;

    if (p->audio_engine_) p->audio_engine_->pause();
    if (p->video_engine_) p->video_engine_->pause();

    p->state_.store(PRISM_STATE_PAUSED);
    spdlog::info("[PrismPlayer] paused");
    return PRISM_OK;
}

int prism_player_stop(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->audio_engine_) {
        p->audio_engine_->pause();
        p->audio_engine_->close();
    }
    if (p->video_engine_) {
        p->video_engine_->pause();
        p->video_engine_->close();
    }

    p->engines_initialized_ = false;
    p->state_.store(PRISM_STATE_STOPPED);
    spdlog::info("[PrismPlayer] stopped");
    return PRISM_OK;
}

int prism_player_seek(PrismPlayerHandle player, int64_t position_ms,
                      PrismSeekMode mode)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        p->last_error_.store(PRISM_ERROR_NO_MEDIA);
        return PRISM_ERROR_NO_MEDIA;
    }

    uint64_t target_pts = static_cast<uint64_t>(position_ms);
    if (mode == PRISM_SEEK_RELATIVE) {
        int64_t cur = prism_player_get_position(player);
        target_pts = static_cast<uint64_t>(std::max<int64_t>(0, cur + position_ms));
    }

    int seek_flag = (mode == PRISM_SEEK_ABSOLUTE) ? 0 : 1;

    if (p->audio_engine_) p->audio_engine_->seek(target_pts, seek_flag);
    if (p->video_engine_) p->video_engine_->seek(target_pts, seek_flag);

    p->fire_event(PRISM_EVENT_SEEK_COMPLETED);
    spdlog::info("[PrismPlayer] seek to {}ms", target_pts);
    return PRISM_OK;
}

PrismState prism_player_get_state(PrismPlayerHandle player)
{
    if (!player) return PRISM_STATE_ERROR;
    return reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player)->state_.load();
}

int64_t prism_player_get_position(PrismPlayerHandle player)
{
    if (!player) return -1;
    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) return -1;

    if (p->audio_engine_) {
        auto info = p->audio_engine_->get_sync_info();
        return static_cast<int64_t>(info.current_pts.load());
    }
    return -1;
}

int64_t prism_player_get_duration(PrismPlayerHandle player)
{
    if (!player) return -1;
    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);
    return p->media_info_.duration_ms;
}

int prism_player_set_volume(PrismPlayerHandle player, float volume)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);
    float clamped = Prism::Service::clamp(volume, 0.0f, 1.0f);
    p->volume_.store(clamped);

    if (!p->mute_.load()) {
        // 实际音量设置由 AudioEngine 处理
    }

    return PRISM_OK;
}

float prism_player_get_volume(PrismPlayerHandle player)
{
    if (!player) return 0.0f;
    return reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player)->volume_.load();
}

int prism_player_set_mute(PrismPlayerHandle player, bool mute)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);

    bool was_muted = p->mute_.exchange(mute);
    if (mute && !was_muted) {
        p->volume_before_mute_ = p->volume_.load();
    } else if (!mute && was_muted) {
        p->volume_.store(p->volume_before_mute_);
    }

    return PRISM_OK;
}

bool prism_player_get_mute(PrismPlayerHandle player)
{
    if (!player) return false;
    return reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player)->mute_.load();
}

int prism_player_set_playback_speed(PrismPlayerHandle player, float speed)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);
    float clamped = Prism::Service::clamp(speed, 0.5f, 2.0f);
    p->speed_.store(clamped);

    if (p->audio_engine_) p->audio_engine_->set_play_speed();
    if (p->video_engine_) p->video_engine_->set_play_speed(clamped);

    return PRISM_OK;
}

float prism_player_get_playback_speed(PrismPlayerHandle player)
{
    if (!player) return 1.0f;
    return reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player)->speed_.load();
}

int prism_player_set_loop(PrismPlayerHandle player, bool loop)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player)->loop_.store(loop);
    return PRISM_OK;
}

bool prism_player_get_loop(PrismPlayerHandle player)
{
    if (!player) return false;
    return reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player)->loop_.load();
}

int prism_player_set_video_window(PrismPlayerHandle player, void* native_window)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player)->video_window_ = native_window;
    return PRISM_OK;
}

int prism_player_get_media_info(PrismPlayerHandle player, PrismMediaInfo* info)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    if (!info)  return PRISM_ERROR_INVALID_PARAM;

    auto* p = reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        p->last_error_.store(PRISM_ERROR_NO_MEDIA);
        return PRISM_ERROR_NO_MEDIA;
    }

    *info = p->media_info_;
    return PRISM_OK;
}

PrismErrorCode prism_player_get_last_error(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    return reinterpret_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.load();
}

const char* prism_player_get_version(void)
{
    return "PrismPlayer " STRINGIFY(PRISM_VERSION_MAJOR) "."
           STRINGIFY(PRISM_VERSION_MINOR) "."
           STRINGIFY(PRISM_VERSION_PATCH);
}

} // extern "C"
```

其中 `STRINGIFY` 宏需要定义。在文件最顶部（`#include "PlayerImpl.h"` 之前）添加：

```cpp
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
```

- [ ] **Step 2: Commit**

```bash
git add client/service/src/Player.cpp
git commit -m "feat(service): implement all 22 C API functions and PlayerImpl class"
```

---

### Task 6: 构建验证

**Files:** 无修改，仅验证

- [ ] **Step 1: 删除旧的占位 tmp 文件（如果存在且导致冲突）**

```bash
rm -f client/service/src/tmp.cpp client/service/Impl/tmp.h client/service/include/tmp.h 2>/dev/null; echo "done"
```

- [ ] **Step 2: 执行 Debug 构建**

```bash
cmake --preset windows-x64-Debug && cmake --build --preset windows-x64-Debug
```

Expected: 构建成功，无编译错误。

- [ ] **Step 3: 验证 DLL 导出符号**

```bash
dumpbin /EXPORTS build/windows-x64-Debug/client/service/client-service.dll 2>/dev/null | grep prism_player || echo "check build output dir for DLL"
```

Expected: 导出的 DLL 中包含全部 22 个 `prism_player_*` 函数。

- [ ] **Step 4: Commit any cleanup**

```bash
git status
# 如有删除的 tmp 文件，一并提交
git add -u
git commit -m "chore(service): remove old placeholder tmp files"
```
