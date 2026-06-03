#pragma once

#include <cstdint>
#include "AudioEngine.h"
#include "VideoEngine.h"

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
     * @brief 下发跳转指令
     * @param pts 目标时间戳（ms）
     * @param seek_mode 跳转模式，0=绝对，1=相对
     * @return 成功返回 true
     */
    virtual bool dispatch_seek(uint64_t pts, int seek_mode) = 0;

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
    virtual bool initialize_engines(void* audio_factory, void* video_factory, bool enable_video) = 0;

    /**
     * @brief 关闭并销毁所有引擎实例
     */
    virtual void shutdown_engines() = 0;
};

} // namespace Prism::Business
