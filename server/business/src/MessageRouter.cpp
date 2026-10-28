#include "MessageRouter.h"

#include "IClientNotifier.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace Prism::Server::Business {

MessageRouter::MessageRouter(Adapter::DBAdapter* db, IClientNotifier* notifier)
    : db_(db), notifier_(notifier) {}

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

    nlohmann::json outbound = message;
    outbound["sender"] = sender_user_id;
    notifier_->broadcast_to_room(room_id, outbound.dump());
}

} // namespace Prism::Server::Business
