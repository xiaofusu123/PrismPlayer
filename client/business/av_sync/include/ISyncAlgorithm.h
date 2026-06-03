#pragma once

#include "SyncTypes.h"

namespace Prism::Business {

/**
 * @class ISyncAlgorithm
 * @brief 音视频同步算法抽象接口
 *
 * 以音频时钟为主时钟，计算视频帧与音频时钟的漂移值，
 * 输出渲染决策（RENDER / WAIT / DROP）。
 */
class ISyncAlgorithm {
public:
    virtual ~ISyncAlgorithm() = default;

    /**
     * @brief 校准音视频同步
     * @param audio_pts 当前音频帧 PTS（ms）
     * @param video_pts 当前视频帧 PTS（ms）
     * @return SyncAction 渲染决策
     */
    virtual SyncAction calibrate(uint64_t audio_pts, uint64_t video_pts) = 0;

    /**
     * @brief 配置同步参数
     * @param config 同步配置
     */
    virtual void configure(const SyncConfig& config) = 0;

    /**
     * @brief 获取当前漂移信息
     * @return DriftInfo 漂移信息
     */
    virtual DriftInfo get_drift_info() const = 0;

    /**
     * @brief 重置同步算法状态
     */
    virtual void reset() = 0;
};

} // namespace Prism::Business
