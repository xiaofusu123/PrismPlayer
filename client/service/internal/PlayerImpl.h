#pragma once

#include "Player.h"

#include "IPlaybackStateMachine.h"
#include "ISyncAlgorithm.h"
#include "ICommandDispatcher.h"
#include "IEngineObserver.h"
#include "IServiceNetwork.h"

#include <memory>
#include <atomic>
#include <string>

namespace Prism::Service {

/**
 * @class PrismPlayerInternal
 * @brief 播放器内部状态，封装 C API 句柄背后的全部数据
 */
class PrismPlayerInternal {
public:
    explicit PrismPlayerInternal(const PrismConfig& cfg,
                                 PrismEventCallback cb,
                                 void* ud);
    ~PrismPlayerInternal();

    void fire_event(PrismEventType type, const void* data = nullptr) const;

    /* ---- 引擎工厂（外部注入，不持有所有权，void* 解耦） ---- */
    void* audio_factory_{nullptr};
    void* video_factory_{nullptr};

    /* ---- AV Sync 组件 ---- */
    std::unique_ptr<Prism::Business::IPlaybackStateMachine> sync_sm_;
    std::unique_ptr<Prism::Business::ISyncAlgorithm>      sync_algo_;
    std::unique_ptr<Prism::Business::ICommandDispatcher>  cmd_dispatcher_;
    std::unique_ptr<Prism::Business::IEngineObserver>     engine_observer_;

    /* ---- 网络抽象（外部注入或默认桩实现） ---- */
    std::unique_ptr<IServiceNetwork> network_;

    /* ---- 媒体信息缓存 ---- */
    PrismMediaInfo media_info_{};

    /* ---- 配置与回调 ---- */
    PrismConfig        config_{};
    std::string        log_level_{"info"};
    PrismEventCallback callback_{nullptr};
    void*              user_data_{nullptr};

    /* ---- 播放状态 ---- */
    std::atomic<PrismState> state_{PRISM_STATE_IDLE};

    /* ---- 音频属性 ---- */
    std::atomic<float> volume_{1.0f};
    std::atomic<bool>  mute_{false};
    std::atomic<float> volume_before_mute_{1.0f};

    /* ---- 播放属性 ---- */
    std::atomic<float> speed_{1.0f};
    std::atomic<bool>  loop_{false};

    /* ---- 视频窗口 ---- */
    std::atomic<void*> video_window_{nullptr};

    /* ---- 错误诊断 ---- */
    std::atomic<PrismErrorCode> last_error_{PRISM_OK};

    /* ---- 媒体 URI ---- */
    std::string media_uri_;

    /* ---- 引擎初始化标记 ---- */
    std::atomic<bool> engines_initialized_{false};
};

} // namespace Prism::Service
