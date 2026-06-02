#pragma once

#include "API.h"
#include "av_synchronizer.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace Prism::Business {
namespace impl {

/* ========== 内部常量 ========== */

/** 最大允许同步漂移（微秒），超过则硬同步 */
constexpr int64_t MAX_SYNC_DRIFT_US = 500000;
/** 音频时钟平滑窗口大小 */
constexpr int     AUDIO_CLOCK_WINDOW = 16;
/** 默认帧间隔（毫秒），用于估算 */
constexpr int64_t DEFAULT_FRAME_INTERVAL_MS = 33;

/* ========== 内部辅助函数 ========== */

/**
 * @brief 计算当前时钟的推算 PTS（考虑时间流逝和播放速度）
 * @param clock   时钟基准
 * @param now_ms  当前系统时间（毫秒）
 * @param speed   播放速度倍率
 * @return 推算的当前 PTS（毫秒）
 */
inline int64_t compute_clock_pts(const SyncClock& clock,
                                 int64_t now_ms,
                                 double speed)
{
    if (clock.paused || clock.last_sys_time_ms == 0 || speed <= 0.0) {
        return clock.pts_ms;
    }
    int64_t elapsed = now_ms - clock.last_sys_time_ms;
    int64_t delta = static_cast<int64_t>(static_cast<double>(elapsed) * speed);
    return clock.pts_ms + delta;
}

/**
 * @brief 钳位值到指定范围
 */
template <typename T>
inline T clamp_value(T val, T lo, T hi)
{
    return std::max(lo, std::min(hi, val));
}

/**
 * @brief 检查是否需要丢弃视频帧
 * @param video_delay_ms calc_video_delay 的结果
 * @param frame_drop_threshold_ms 丢弃阈值
 * @return true 表示应丢弃该帧
 */
inline bool should_drop_frame(int64_t video_delay_ms,
                              int64_t frame_drop_threshold_ms)
{
    return video_delay_ms < 0
        && std::abs(video_delay_ms) > frame_drop_threshold_ms;
}

} // namespace impl
} // namespace Prism::Business
