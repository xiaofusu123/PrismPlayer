#pragma once

#include "AudioEngine.h"
#include "IEngineObserver.h"
#include "SyncTypes.h"
#include "VideoEngine.h"
#include <chrono>
#include <cstdint>

namespace Prism::Business {

/**
 * @class ICommandDispatcher
 * @brief 向 Engine 层分发播放指令的抽象接口
 *
 * 封装对 AudioEngine/VideoEngine 的直接调用，
 * 使上层（Service）不需要直接操作 Engine。
 */
class ICommandDispatcher {
  public:
    virtual ~ICommandDispatcher() = default;

    /**
     * @brief 下发播放指令
     * @return 成功返回 true
     */
    virtual bool dispatch_play() = 0;

    /**
     * @brief 下发暂停指令
     * @return 成功返回 true
     */
    virtual bool dispatch_pause() = 0;

    /**
     * @brief 下发跳转指令（只收绝对位置）
     * @param pts 目标位置，相对流起点
     *
     * 相对快进/倒退由 Service 层换算成绝对位置后再调用，引擎不持有 seek 模式。
     * @return 成功返回 true
     */
    virtual bool dispatch_seek(std::chrono::milliseconds pts) = 0;

    /**
     * @brief 下发倍速指令
     * @param speed 播放速度倍率
     * @return 成功返回 true
     */
    virtual bool dispatch_speed(float speed) = 0;

    /**
     * @brief 绑定音频引擎
     * @param engine 音频引擎指针（不持有所有权）
     */
    virtual void set_audio_engine(Prism::Engine::AudioEngine* engine) = 0;

    /**
     * @brief 绑定视频引擎
     * @param engine 视频引擎指针（不持有所有权）
     */
    virtual void set_video_engine(Prism::Engine::VideoEngine* engine) = 0;

    /**
     * @brief 从工厂创建并初始化引擎，接管引擎所有权
     * @param audio_factory 音频引擎工厂指针（void* 解耦），nullptr 则不创建音频引擎
     * @param video_factory 视频引擎工厂指针（void* 解耦），nullptr 则不创建视频引擎
     * @param enable_video 是否启用视频引擎
     * @return 成功返回 true
     */
    virtual bool initialize_engines(void* audio_factory, void* video_factory,
                                    bool enable_video) = 0;

    /**
     * @brief 绑定帧观察者，供引擎初始化时注册（不持有所有权）
     * @param observer 观察者指针；nullptr 表示注销。注册/注销不在帧回调内执行。
     */
    virtual void set_frame_observer(IEngineObserver* observer) = 0;

    /**
     * @brief 采集引擎侧媒体属性快照，供上层回填媒体信息
     * @return MediaSnapshot 各字段为 0 表示引擎尚未提供该属性
     *
     * 只读查询：不触发回调、不改变播放状态，可在 open 成功后调用。
     */
    [[nodiscard]] virtual MediaSnapshot get_media_snapshot() const = 0;

    /**
     * @brief 关闭并销毁所有引擎实例（先注销观察者，之后再无帧回调）
     */
    virtual void shutdown_engines() = 0;
};

} // namespace Prism::Business
