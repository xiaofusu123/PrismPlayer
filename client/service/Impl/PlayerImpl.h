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
