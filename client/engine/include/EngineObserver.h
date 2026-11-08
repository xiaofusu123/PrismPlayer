#pragma once

#include "AudioEngine.h"
#include "VideoEngine.h"

namespace Prism::Engine {

/**
 * @class EngineObserver
 * @brief 引擎帧回调观察者接口
 *
 * 由引擎层对外声明，供同步侧（Business 层）实现并在装配时注册。
 * 引擎在每一帧就绪时回调对应方法，从而把 PTS 真正送进 AV Sync。
 *
 * 线程语义与重入约束（实现方必须遵守）：
 * - 这是热路径：回调可能每帧触发，禁止在其中分配内存、加锁阻塞或做 I/O；
 * - 回调可能来自引擎的解码/渲染线程，实现必须是可重入且线程安全的；
 * - 回调不得反向调用引擎（避免与引擎内部锁发生死锁）。
 */
class EngineObserver {
  public:
    virtual ~EngineObserver() = default;

    /**
     * @brief 音频帧就绪回调
     * @param info 音频同步信息
     */
    virtual void on_audio_frame(const AudioSyncInfo& info) = 0;

    /**
     * @brief 视频帧就绪回调
     * @param info 视频同步信息
     */
    virtual void on_video_frame(const VideoSyncInfo& info) = 0;

    /**
     * @brief 渲染就绪回调
     * @param metadata 渲染元数据
     */
    virtual void on_render_ready(const RenderMetadata& metadata) = 0;
};

} // namespace Prism::Engine
