#pragma once

#include "API.h"
#include "Player.h"
#include "av_synchronizer.h"

#include "AudioEngine.h"
#include "AudioEngineFactory.h"
#include "AudioEngineWasapiSharedFactory.h"
#include "VideoEngine.h"
#include "VideoEngineFactory.h"
#include "VideoEngineVulkanFactory.h"

#include <memory>
#include <atomic>
#include <string>
#include <chrono>

namespace Prism::Service {

class PrismPlayerInternal {
public:
    explicit PrismPlayerInternal(const PrismConfig& cfg,
                                 PrismEventCallback cb,
                                 void* ud);
    ~PrismPlayerInternal();

    void fire_event(PrismEventType type, const void* data = nullptr) const;

    /**
     * @brief 获取当前系统时间（毫秒），供同步器使用
     */
    static int64_t system_time_ms();

    // 引擎工厂
    Prism::Engine::AudioEngineWasapiSharedFactory audio_factory_;
    Prism::Engine::VideoEngineVulkanFactory         video_factory_;

    // 引擎实例（延迟创建：首次 open 时 init）
    std::unique_ptr<Prism::Engine::AudioEngine> audio_engine_;
    std::unique_ptr<Prism::Engine::VideoEngine> video_engine_;

    // 音视频同步器（业务层）
    Prism::Business::SyncConfig  sync_config_;
    Prism::Business::AVSynchronizer sync_;

    // 媒体信息缓存
    PrismMediaInfo media_info_;

    // 配置
    PrismConfig config_;
    std::string log_level_;

    // 播放状态
    std::atomic<PrismState> state_{PRISM_STATE_IDLE};

    // 音频属性
    std::atomic<float> volume_{1.0f};
    std::atomic<bool>  mute_{false};
    std::atomic<float> volume_before_mute_{1.0f};

    // 播放属性
    std::atomic<float> speed_{1.0f};
    std::atomic<bool>  loop_{false};

    // 视频窗口
    std::atomic<void*> video_window_{nullptr};

    // 错误诊断
    std::atomic<PrismErrorCode> last_error_{PRISM_OK};

    // 媒体 URI（用于重新 open）
    std::string media_uri_;

    // 是否已初始化引擎
    std::atomic<bool> engines_initialized_{false};

private:
    PrismEventCallback callback_;
    void*              user_data_;
};

} // namespace Prism::Service
