#include "PlaybackStateMachine.h"

#include <spdlog/spdlog.h>

namespace Prism::Business {

PlaybackStateMachine::PlaybackStateMachine()
{
    state_.store(SyncState::UNINIT);
}

bool PlaybackStateMachine::transition(SyncState target)
{
    SyncState current = state_.load();

    if (current == target) {
        return true;
    }

    if (!is_valid_transition(current, target)) {
        spdlog::warn("[PlaybackStateMachine] invalid transition: {} -> {}",
                     static_cast<int>(current), static_cast<int>(target));
        return false;
    }

    state_.store(target);
    spdlog::debug("[PlaybackStateMachine] state: {} -> {}",
                  static_cast<int>(current), static_cast<int>(target));
    return true;
}

SyncState PlaybackStateMachine::get_state() const
{
    return state_.load();
}

bool PlaybackStateMachine::can_play() const
{
    SyncState s = state_.load();
    return s == SyncState::SYNCHRONIZED || s == SyncState::AHEAD ||
           s == SyncState::BEHIND || s == SyncState::DISABLE;
}

bool PlaybackStateMachine::can_seek() const
{
    SyncState s = state_.load();
    return s != SyncState::UNINIT && s != SyncState::ERROR;
}

void PlaybackStateMachine::reset()
{
    state_.store(SyncState::UNINIT);
    spdlog::debug("[PlaybackStateMachine] reset to UNINIT");
}

bool PlaybackStateMachine::is_valid_transition(SyncState from, SyncState to) const
{
    switch (to) {
    case SyncState::UNINIT:
        return true;
    case SyncState::CALIBATING:
        return from == SyncState::UNINIT || from == SyncState::SYNCHRONIZED;
    case SyncState::SYNCHRONIZED:
        return from == SyncState::CALIBATING || from == SyncState::AHEAD ||
               from == SyncState::BEHIND;
    case SyncState::AHEAD:
    case SyncState::BEHIND:
        return from == SyncState::SYNCHRONIZED || from == SyncState::AHEAD ||
               from == SyncState::BEHIND;
    case SyncState::DISABLE:
        return from == SyncState::UNINIT;
    case SyncState::ERROR:
        return true;
    }
    return false;
}

} // namespace Prism::Business
