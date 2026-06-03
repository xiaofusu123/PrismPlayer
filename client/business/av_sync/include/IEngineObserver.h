#pragma once

#include "AudioEngine.h"
#include "VideoEngine.h"

namespace Prism::Business {

class ISyncAlgorithm;
class IPlaybackStateMachine;

/**
 * @class IEngineObserver
 * @brief 引擎帧回调观察者抽象接口
 *
 * 接收 Engine 层的帧回调，驱动同步算法校准。
 * 在帧到达时触发 calibrate，根据结果调整渲染行为。
 */
class IEngineObserver {
public:
    virtual ~IEngineObserver() = default;

    /**
     * @brief 音频帧到达回调
     * @param info 音频同步信息
     */
    virtual void on_audio_frame(const Prism::Engine::AudioSyncInfo& info) = 0;

    /**
     * @brief 视频帧到达回调
     * @param info 视频同步信息
     */
    virtual void on_video_frame(const Prism::Engine::VideoSyncInfo& info) = 0;

    /**
     * @brief 渲染就绪回调
     * @param metadata 渲染元数据
     */
    virtual void on_render_ready(const Prism::Engine::RenderMetadata& metadata) = 0;

    /**
     * @brief 绑定同步算法
     * @param algo 同步算法指针（不持有所有权）
     */
    virtual void set_sync_algorithm(ISyncAlgorithm* algo) = 0;

    /**
     * @brief 绑定状态机
     * @param sm 状态机指针（不持有所有权）
     */
    virtual void set_state_machine(IPlaybackStateMachine* sm) = 0;
};

} // namespace Prism::Business
