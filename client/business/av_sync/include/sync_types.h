#pragma once

#include "API.h"

#include <cstdint>

namespace Prism::Business {

/* ========== 同步模式 ========== */

/**
 * @brief 音视频同步主时钟模式
 */
enum SyncMode {
    SYNC_MODE_AUDIO_MASTER   = 0,
    SYNC_MODE_VIDEO_MASTER   = 1,
    SYNC_MODE_EXTERNAL_CLOCK = 2
};

/* ========== 同步时钟 ========== */

/**
 * @struct SyncClock
 * @brief 同步时钟，记录当前 PTS 及系统参考时间
 */
struct SyncClock {
    int64_t pts_ms{0};
    int64_t last_sys_time_ms{0};
    double  drift_ppm{0.0};
    bool    paused{false};
};

/* ========== 同步结果句柄 ========== */

/**
 * @struct SyncResult
 * @brief 同步结果，作为传递给上层的统一句柄
 */
struct SyncResult {
    int64_t audio_pts_ms{0};
    int64_t video_pts_ms{0};
    int64_t master_pts_ms{0};
    double  sync_drift_ms{0.0};
    bool    is_synced{false};
};

/* ========== 同步配置 ========== */

/**
 * @struct SyncConfig
 * @brief 同步器配置参数
 */
struct SyncConfig {
    int64_t  max_drift_ms{200};
    int64_t  sync_threshold_ms{50};
    int64_t  frame_drop_threshold_ms{100};
    SyncMode mode{SYNC_MODE_AUDIO_MASTER};
    bool     enable_frame_drop{true};
};

} // namespace Prism::Business
