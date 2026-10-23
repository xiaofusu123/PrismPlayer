#pragma once

#include "IPlaybackStateMachine.h"

#include <atomic>

namespace Prism::Business {

/**
 * @class PlaybackStateMachine
 * @brief 播放状态机实现
 */
class PlaybackStateMachine : public IPlaybackStateMachine {
  public:
    PlaybackStateMachine();
    ~PlaybackStateMachine() override = default;

    bool transition(SyncState target) override;
    [[nodiscard]] SyncState get_state() const override;
    [[nodiscard]] bool can_play() const override;
    [[nodiscard]] bool can_seek() const override;
    void reset() override;

  private:
    std::atomic<SyncState> state_{SyncState::UNINIT};

    [[nodiscard]] bool is_valid_transition(SyncState from, SyncState to) const;
};

} // namespace Prism::Business
