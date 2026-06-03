#pragma once

#include "sync_types.h"

#include <cstdint>

namespace Prism::Business {

/**
 * @class AVSynchronizer
 * @brief 音视频同步器
 *
 * 以音频时钟为主时钟（默认），根据音频播放进度计算视频帧应
 * 延迟或丢弃，最终向上层输出统一的 SyncResult 同步结果句柄。
 */
class AVSynchronizer {
public:
    /**
     * @brief 构造同步器
     * @param config 同步配置（可使用默认值）
     */
    explicit AVSynchronizer(const SyncConfig& config = SyncConfig{});

    ~AVSynchronizer() = default;

    /* ---------- 时钟更新 ---------- */

    /**
     * @brief 更新音频主时钟
     * @param pts_ms       当前音频 PTS（毫秒）
     * @param sys_time_ms  系统参考时间（毫秒）
     */
    void update_audio_clock(int64_t pts_ms, int64_t sys_time_ms);

    /**
     * @brief 更新视频时钟
     * @param pts_ms       当前视频帧 PTS（毫秒）
     * @param sys_time_ms  系统参考时间（毫秒）
     */
    void update_video_clock(int64_t pts_ms, int64_t sys_time_ms);

    /* ---------- 同步查询 ---------- */

    /**
     * @brief 获取当前同步结果（传递给上层）
     * @return SyncResult 统一同步结果句柄
     */
    SyncResult get_sync_result() const;

    /**
     * @brief 获取主时钟当前 PTS
     * @return 主时钟 PTS（毫秒）
     */
    int64_t get_master_pts() const;

    /**
     * @brief 计算视频帧应延迟的时间
     *
     * 当视频帧 PTS 超前于音频时钟时，返回正数表示应等待的毫秒数；
     * 当视频帧落后于音频时钟超过丢弃阈值时，返回负数表示应丢弃该帧。
     *
     * @param video_pts_ms 视频帧 PTS（毫秒）
     * @return 正数=延迟时间（ms），负数=应丢弃，零=立即渲染
     */
    int64_t calc_video_delay(int64_t video_pts_ms) const;

    /* ---------- 控制 ---------- */

    /**
     * @brief 切换同步模式
     * @param mode 目标同步模式
     */
    void set_mode(SyncMode mode);

    /**
     * @brief 获取当前同步模式
     * @return 当前 SyncMode
     */
    SyncMode get_mode() const { return config_.mode; }

    /**
     * @brief 设置播放速度，影响时钟推进速率
     * @param speed 速度倍率，范围 0.5x-2.0x
     */
    void set_play_speed(double speed) { play_speed_ = speed; }

    /**
     * @brief 获取当前播放速度
     * @return 当前速度倍率
     */
    double get_play_speed() const { return play_speed_; }

    /**
     * @brief 获取当前配置
     * @return 当前 SyncConfig 引用
     */
    const SyncConfig& get_config() const { return config_; }

    /**
     * @brief 重置同步器状态
     */
    void reset();

    /**
     * @brief 暂停时钟推进（播放暂停时调用）
     */
    void pause_clock();

    /**
     * @brief 恢复时钟推进（播放恢复时调用）
     */
    void resume_clock(int64_t sys_time_ms);

private:
    SyncConfig config_;
    SyncClock  audio_clock_;
    SyncClock  video_clock_;
    SyncClock  master_clock_;

    double     play_speed_{1.0};

    /**
     * @brief 根据当前模式选择主时钟源
     */
    const SyncClock& master_source() const;

    /**
     * @brief 计算时钟当前 PTS（考虑时间流逝）
     * @param clock 时钟
     * @param now_ms 当前系统时间
     * @return 推算的当前 PTS
     */
    int64_t clock_pts(const SyncClock& clock, int64_t now_ms) const;
};

} // namespace Prism::Business
