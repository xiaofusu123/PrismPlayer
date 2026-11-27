#include "SystemSyncClock.h"

namespace Prism::Business {

SystemSyncClock::SystemSyncClock() : origin_(std::chrono::steady_clock::now()) {}

uint64_t SystemSyncClock::now_ms() const {
    auto elapsed = std::chrono::steady_clock::now() - origin_;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

} // namespace Prism::Business
