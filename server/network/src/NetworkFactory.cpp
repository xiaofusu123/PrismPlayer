#include "NetworkFactory.h"

#include "IxWebSocketServer.h"

namespace Prism::Server::Network {

std::unique_ptr<ISignalingServer> create_signaling_server() {
    return std::make_unique<IxWebSocketServer>();
}

} // namespace Prism::Server::Network
