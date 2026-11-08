#include "MessageRouter.h"

#include "DBAdapter.h"
#include "IClientNotifier.h"
#include "IRoomManager.h"
#include "ServerTypes.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace Prism::Server::Business {

MessageRouter::MessageRouter(Adapter::DBAdapter* db, IRoomManager* rooms, IClientNotifier* notifier)
    : db_(db), rooms_(rooms), notifier_(notifier) {}

void MessageRouter::route(int64_t sender_user_id, const std::string& json) {
    nlohmann::json message;
    try {
        message = nlohmann::json::parse(json);
    } catch (const nlohmann::json::exception& e) {
        spdlog::warn("route: invalid JSON from user {}: {}", sender_user_id, e.what());
        return;
    }

    if (!message.contains("room_id") || !message["room_id"].is_string()) {
        spdlog::warn("route: message from user {} missing room_id", sender_user_id);
        return;
    }

    std::string room_id = message["room_id"].get<std::string>();

    if (message.value("type", "") == MSG_PLAYBACK && rooms_ != nullptr) {
        update_playback_state(room_id, message);
    }

    nlohmann::json outbound = message;
    outbound["sender"] = sender_user_id;
    notifier_->broadcast_to_room(room_id, outbound.dump());
}

void MessageRouter::update_playback_state(const std::string& room_id,
                                          const nlohmann::json& message) {
    auto room = db_->query_room_by_id(room_id);
    if (!room.has_value()) {
        spdlog::warn("route: playback for unknown room '{}'", room_id);
        return;
    }

    Adapter::RoomState state = room->state;
    std::string action = message.value("action", "");
    if (action == PLAYBACK_ACTION_PLAY) {
        state = Adapter::RoomState::PLAYING;
    } else if (action == PLAYBACK_ACTION_PAUSE) {
        state = Adapter::RoomState::PAUSED;
    } else if (action == PLAYBACK_ACTION_STOP) {
        state = Adapter::RoomState::IDLE;
    }

    int64_t position_ms = message.value("position_ms", 0);
    rooms_->update_room_state(room_id, state, position_ms);
}

} // namespace Prism::Server::Business
