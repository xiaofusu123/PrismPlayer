#pragma once

#include <atomic>

namespace Prism::Engine {

/**
 * @struct AudioSyncInfo
 * @brief 音频同步信息
 */
struct AudioSyncInfo {
    std::atomic<uint64_t> current_pts{0};  /**< 当前音频帧解码时间戳（ms） */
    std::atomic<uint64_t> next_pts{0};     /**< 下一帧音频解码时间戳（ms） */
    std::atomic<uint32_t> start_time{0};   /**< 流开始的时间 */
    std::atomic<uint64_t> draution{0};     /**< 音频总时长 */
};


class AudioEngine {
public:
    virtual ~AudioEngine() = default;

    /**
    * @brief 初始化音频引擎
    */
    virtual bool init() = 0;

    /**
    * @brief 播放音频
    */
    virtual bool play() = 0;

    /**
    * @brief 暂停音频
    */
    virtual bool pause() = 0;

    /**
    * @brief 关闭音频
    */
    virtual bool close() = 0;

    /**
    * @brief 设置播放速度（倍速播放）
    */
    virtual bool set_play_speed() = 0;

    /**
    * @brief 音频跳转
    * @param pts 跳转的时间戳
    * @param seek_mode 跳转模式。0为绝对模式，直接跳转到音频对应的时间，此时pts为非负值；1为相对时间，快进或倒退对应时间
    */
    virtual bool seek(uint64_t pts, int seek_mode) = 0;


    /**
    * @brief 获取音频同步信息
    * @return AudioSyncInfo 音频同步信息
    */
    virtual AudioSyncInfo get_sync_info() = 0;
};

} // namespace Prism::Engine
