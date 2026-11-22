#pragma once

#include <atomic>
#include <chrono>

namespace Prism::Engine {

/**
 * @struct AudioSyncInfo
 * @brief 音频同步信息，由音频引擎写、AV Sync 层读，跨线程交换
 *
 * 成员都是 atomic：只在同一进程内的线程之间使用，不序列化、不发给其它进程，
 * 也不构成任何 ABI 稳定性承诺（改类型/加字段不需要兼容旧二进制）。
 * 因为含 atomic，本结构不可拷贝、不可移动；取用时只读需要的成员。
 *
 * 单位统一为 std::chrono::milliseconds：
 * - current_pts / next_pts：相对流起点的播放位置，绝对时间轴上的毫秒数；
 * - start_time：流开始时刻相对文件起点的偏移（旧实现用 uint32_t 存毫秒，
 *   超过约 49.7 天会回绕；改成 milliseconds 后由 64 位计数承载，见 #128）；
 * - duration：音频总时长（旧实现为 uint64_t 毫秒，语义不变）。
 */
struct AudioSyncInfo {
    std::atomic<std::chrono::milliseconds> current_pts{}; /**< 当前音频帧的解码时间戳 */
    std::atomic<std::chrono::milliseconds> next_pts{};    /**< 下一帧音频帧的解码时间戳 */
    std::atomic<std::chrono::milliseconds> start_time{};  /**< 流开始时刻相对文件起点的偏移 */
    std::atomic<std::chrono::milliseconds> duration{};    /**< 音频总时长 */
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
     * @param speed 播放速率，1.0 为原速，>1 加速、<1 减速；必须为正数
     */
    virtual bool set_play_speed(float speed) = 0;

    /**
     * @brief 音频跳转
     * @param pts 跳转目标位置（绝对位置，相对流起点）
     *
     * 只接受绝对位置：相对快进/倒退由上层（Service / AV Sync）换算成绝对位置后再调用。
     */
    virtual bool seek(std::chrono::milliseconds pts) = 0;

    /**
     * @brief 获取音频同步信息
     * @return AudioSyncInfo 音频同步信息
     */
    virtual AudioSyncInfo get_sync_info() = 0;
};

} // namespace Prism::Engine
