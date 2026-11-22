#pragma once

#include <cstdint>

namespace Prism::Business {

/**
 * @class ISyncClock
 * @brief 同步侧时钟抽象（虚拟时钟注入点）
 *
 * 设计意图（对应 #134）：
 *   - 同步用例不依赖真实墙钟：测试注入可控的虚拟时钟推进逻辑时间，
 *     同一用例连跑 3 次结果一致；
 *   - 生产环境注入基于 steady_clock 的系统时钟；
 *   - drift 永远由 PTS 之差计算（video_pts - audio_pts），本时钟只用于
 *     校准阶段的时长判定，不参与 drift 计算 —— 因此暂停期间即便时钟推进，
 *     只要没有新的音视频帧回调，drift 也不会随墙钟增长。
 */
class ISyncClock {
  public:
    virtual ~ISyncClock() = default;

    /**
     * @brief 取当前逻辑时间（ms）
     */
    [[nodiscard]] virtual uint64_t now_ms() const = 0;
};

} // namespace Prism::Business
