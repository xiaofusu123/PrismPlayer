#pragma once

#include <cstdint>

namespace Prism::Business {

/**
 * @enum SyncState
 * @brief 音频同步状态
 *
 * - UNINIT: 音频引擎未初始化
 * - CALIBATING: 音频校准中（初始缓冲阶段）
 * - SYNCHRONIZED: 音频已同步
 * - AHEAD: 音频时间超前
 * - BEHIND: 音频时间滞后
 * - DISABLE: 未启用同步控制（纯本地播放）
 * - SYNC_ERROR: 错误状态
 */
enum class SyncState { UNINIT, CALIBATING, SYNCHRONIZED, AHEAD, BEHIND, DISABLE, SYNC_ERROR };

/**
 * @enum SyncAction
 * @brief 同步算法输出的渲染决策
 */
enum class SyncAction {
    RENDER, /**< 正常渲染当前帧 */
    WAIT,   /**< 视频超前，等待音频追赶 */
    DROP    /**< 视频滞后，丢弃当前帧 */
};

/**
 * @struct DriftInfo
 * @brief 音视频漂移信息
 */
struct DriftInfo {
    int64_t drift_ms{0};   /**< 音视频漂移值（ms），正数=视频超前 */
    uint64_t audio_pts{0}; /**< 当前音频 PTS（ms） */
    uint64_t video_pts{0}; /**< 当前视频 PTS（ms） */
};

/**
 * @struct SyncConfig
 * @brief 同步算法配置参数
 */
struct SyncConfig {
    uint32_t ahead_threshold_ms{30};     /**< 视频超前超过此值则 WAIT */
    uint32_t behind_threshold_ms{50};    /**< 视频滞后超过此值则 DROP */
    uint32_t calibrate_duration_ms{500}; /**< 初始校准缓冲时长（ms） */
};

/**
 * @struct MediaSnapshot
 * @brief 引擎侧媒体属性快照，供 Service 回填 PrismMediaInfo
 *
 * 普通聚合类型，不含 atomic，也不参与 drift 计算；由 CommandDispatcher
 * 在引擎已初始化之后采集。字段为 0 表示引擎尚未提供该属性。
 */
struct MediaSnapshot {
    int64_t audio_duration_ms{0}; /**< 音频总时长（ms），0 表示未知 */
    int64_t video_duration_ms{0}; /**< 视频总时长（ms），0 表示未知 */
    uint32_t video_width{0};      /**< 视频宽度（像素），0 表示未知 */
    uint32_t video_height{0};     /**< 视频高度（像素），0 表示未知 */
};

} // namespace Prism::Business
