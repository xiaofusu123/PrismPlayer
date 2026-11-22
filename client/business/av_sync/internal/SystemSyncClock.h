#pragma once

#include "ISyncClock.h"

#include <chrono>

namespace Prism::Business {

/**
 * @class SystemSyncClock
 * @brief 基于 steady_clock 的生产时钟，从构造时刻起算的相对毫秒
 */
class SystemSyncClock : public ISyncClock {
  public:
    SystemSyncClock();
    ~SystemSyncClock() override = default;

    [[nodiscard]] uint64_t now_ms() const override;

  private:
    std::chrono::steady_clock::time_point origin_;
};

} // namespace Prism::Business
