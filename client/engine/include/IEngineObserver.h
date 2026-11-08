#pragma once

#include "AudioEngine.h"
#include "VideoEngine.h"

namespace Prism::Engine {

/**
 * @class IEngineObserver
 * @brief 引擎帧回调观察者接口
 *
 * 由引擎层对外声明定义，由同步侧（Business/AV Sync）实现并在装配时注册。
 * 引擎在每一帧就绪时回调本接口，驱动 AV Sync 校准：
 *   - 音频帧就绪 → on_audio_frame
 *   - 视频帧就绪 → on_video_frame
 *   - 渲染完成   → on_render_ready
 *
 * 线程语义与重入约束：
 *   - 回调发生在引擎的解码/渲染线程，属于热路径；
 *   - 实现不得在回调内分配内存、加锁阻塞，也不得反向调用引擎方法
 *     （禁止从回调内再次调用 AudioEngine/VideoEngine 的任意方法，以免重入）；
 *   - 回调可能与引擎的 play/pause/seek/close 并发，实现须自行保证数据竞争安全。
 */
class IEngineObserver {
  public:
    virtual ~IEngineObserver() = default;

    /**
     * @brief 音频帧到达回调
     * @param info 音频同步信息（含当前音频 PTS）
     */
    virtual void on_audio_frame(const AudioSyncInfo& info) = 0;

    /**
     * @brief 视频帧到达回调
     * @param info 视频同步信息（含当前视频 PTS）
     */
    virtual void on_video_frame(const VideoSyncInfo& info) = 0;

    /**
     * @brief 渲染就绪回调
     * @param metadata 渲染元数据
     */
    virtual void on_render_ready(const RenderMetadata& metadata) = 0;
};

} // namespace Prism::Engine
