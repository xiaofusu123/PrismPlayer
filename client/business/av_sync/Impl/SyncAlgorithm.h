#pragma once

#include "ISyncAlgorithm.h"

namespace Prism::Business {

/**
 * @class SyncAlgorithm
 * @brief 音视频同步算法实现
 *
 * 以音频时钟为主时钟，计算 video_pts - audio_pts 漂移值。
 * 正 drift = 视频超前 → WAIT；负 drift 超过阈值 = 视频滞后 → DROP。
 */
class SyncAlgorithm : public ISyncAlgorithm {
public:
    SyncAlgorithm();
    ~SyncAlgorithm() override = default;

    SyncAction calibrate(uint64_t audio_pts, uint64_t video_pts) override;
    void configure(const SyncConfig& config) override;
    DriftInfo get_drift_info() const override;
    void reset() override;

private:
    SyncConfig config_;
    DriftInfo drift_info_;
};

} // namespace Prism::Business
