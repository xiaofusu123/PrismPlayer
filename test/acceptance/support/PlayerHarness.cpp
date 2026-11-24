#include "PlayerHarness.h"

#include <algorithm>

namespace Prism::Qa {

/* ========== EventLog ========== */

void EventLog::callback(PrismEventType type, const void* /*data*/, void* user_data) {
    if (user_data == nullptr) {
        return;
    }
    static_cast<EventLog*>(user_data)->events_.push_back(type);
}

bool EventLog::contains(PrismEventType type) const {
    return std::find(events_.begin(), events_.end(), type) != events_.end();
}

std::size_t EventLog::count(PrismEventType type) const {
    return static_cast<std::size_t>(std::count(events_.begin(), events_.end(), type));
}

/* ========== PlayerGuard ========== */

PlayerGuard::PlayerGuard(const PrismConfig* config, PrismEventCallback callback, void* user_data)
    : handle_(prism_player_create(config, callback, user_data)) {}

PlayerGuard::~PlayerGuard() {
    prism_player_destroy(handle_);
}

/* ========== FakeBackends ========== */

FakeBackends::FakeBackends()
    : audio(std::make_shared<EngineRecorder>()), video(std::make_shared<EngineRecorder>()),
      audio_factory(audio), video_factory(video) {}

} // namespace Prism::Qa
