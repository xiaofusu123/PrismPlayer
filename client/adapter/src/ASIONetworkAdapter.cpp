#include "ASIONetworkAdapter.h"

#include <IXHttp.h>
#include <IXHttpClient.h>
#include <IXNetSystem.h>
#include <IXWebSocket.h>

#include <chrono>
#include <utility>

#include <spdlog/spdlog.h>

namespace Prism::Adapter {
namespace {

// ==================== 超时 ====================

// ixwebsocket 的默认值是「建连 60s / 传输 1800s」，那是给命令行工具准备的。
// 客户端卡住 30 分钟不可接受，这里收紧到人能等到的时间。
constexpr int HTTP_CONNECT_TIMEOUT_SECS = 10;
constexpr int HTTP_TRANSFER_TIMEOUT_SECS = 60;
constexpr int WS_HANDSHAKE_TIMEOUT_SECS = 10;

// ==================== 辅助 ====================

/**
 * @brief 把公共接口的请求头表转成 ixwebsocket 的大小写不敏感 map
 * @param headers 自定义请求头
 * @return ixwebsocket 的请求头表
 */
ix::WebSocketHttpHeaders
to_ix_headers(const std::unordered_map<std::string, std::string>& headers) {
    ix::WebSocketHttpHeaders result;
    for (auto& item : headers) {
        result.emplace(item.first, item.second);
    }
    return result;
}

/**
 * @brief 把 ixwebsocket 的响应转成公共接口的响应
 * @param raw ixwebsocket 的响应
 * @return HttpResponse 响应；输入为空或出错时 `status_code == 0`
 */
HttpResponse to_response(const ix::HttpResponsePtr& raw) {
    HttpResponse response;
    if (!raw) {
        spdlog::error("[NetworkAdapter] HTTP 请求没有返回响应对象");
        return response;
    }

    response.status_code = raw->statusCode;
    response.body = raw->body;
    for (auto& item : raw->headers) {
        response.headers.emplace(item.first, item.second);
    }

    if (raw->errorCode != ix::HttpErrorCode::Ok) {
        spdlog::warn("[NetworkAdapter] HTTP 请求未完成 code={} desc={}",
                     static_cast<int>(raw->errorCode), raw->errorMsg);
    }
    return response;
}

} // namespace

// 构造函数放在这里而不是头文件里的 `= default`：`ws_` 是 `unique_ptr<ix::WebSocket>`，
// 而 ix::WebSocket 在头文件里只有前置声明，就地 default 会触发「不完整类型」的
// static_assert。
ASIONetworkAdapter::ASIONetworkAdapter() = default;

ASIONetworkAdapter::~ASIONetworkAdapter() {
    // 此处对象已是 ASIONetworkAdapter 类型，shutdown() 不会再向下派发。
    shutdown();
}

bool ASIONetworkAdapter::init() {
    if (inited_) {
        return true;
    }
    if (!ix::initNetSystem()) {
        spdlog::error("[NetworkAdapter] 网络子系统初始化失败");
        return false;
    }
    inited_ = true;
    spdlog::debug("[NetworkAdapter] 网络子系统初始化完成");
    return true;
}

void ASIONetworkAdapter::shutdown() {
    ws_close();
    if (!inited_) {
        return;
    }
    ix::uninitNetSystem();
    inited_ = false;
    spdlog::debug("[NetworkAdapter] 网络子系统已释放");
}

HttpResponse
ASIONetworkAdapter::http_get(const std::string& url,
                             const std::unordered_map<std::string, std::string>& headers) {
    return send_http(url, ix::HttpClient::kGet, std::string(), headers);
}

HttpResponse
ASIONetworkAdapter::http_post(const std::string& url, const std::string& body,
                              const std::unordered_map<std::string, std::string>& headers) {
    return send_http(url, ix::HttpClient::kPost, body, headers);
}

HttpResponse
ASIONetworkAdapter::send_http(const std::string& url, const std::string& verb,
                              const std::string& body,
                              const std::unordered_map<std::string, std::string>& headers) {
    if (url.empty()) {
        spdlog::error("[NetworkAdapter] HTTP 请求 URL 为空");
        return HttpResponse{};
    }
    if (!init()) {
        return HttpResponse{};
    }

    ix::HttpClient client(false);
    ix::HttpRequestArgsPtr args = client.createRequest(url, verb);
    args->extraHeaders = to_ix_headers(headers);
    args->connectTimeout = HTTP_CONNECT_TIMEOUT_SECS;
    args->transferTimeout = HTTP_TRANSFER_TIMEOUT_SECS;

    return to_response(client.request(url, verb, body, args));
}

bool ASIONetworkAdapter::on_callback_thread() {
    std::lock_guard<std::mutex> lock(ws_state_.mutex);
    return ws_state_.thread_id != std::thread::id() &&
           ws_state_.thread_id == std::this_thread::get_id();
}

bool ASIONetworkAdapter::ws_connect(const std::string& url, WsCallback* callback) {
    if (url.empty()) {
        spdlog::error("[NetworkAdapter] ws_connect 的 URL 为空");
        return false;
    }
    if (callback == nullptr) {
        spdlog::error("[NetworkAdapter] ws_connect 的回调为空");
        return false;
    }
    if (!init()) {
        return false;
    }

    // 同一个适配器只维持一条连接：先收掉上一条。
    if (!ws_close()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(ws_state_.mutex);
        ws_state_.opened = false;
        ws_state_.failed = false;
        ws_state_.connecting = true;
        ws_state_.error.clear();
        ws_state_.thread_id = std::thread::id();
        ws_state_.callback = callback;
    }

    ix::WebSocket* socket = nullptr;
    {
        std::lock_guard<std::mutex> lock(ws_mutex_);
        ws_ = std::make_unique<ix::WebSocket>();
        socket = ws_.get();
    }

    socket->setUrl(url);
    // 重连策略属于 Business 层，这里一律关掉，避免和上层退避打架。
    socket->disableAutomaticReconnection();
    socket->setHandshakeTimeout(WS_HANDSHAKE_TIMEOUT_SECS);
    socket->setOnMessageCallback(
        [this](const ix::WebSocketMessagePtr& message) { handle_ws_message(message); });
    socket->start();

    std::unique_lock<std::mutex> lock(ws_state_.mutex);
    bool settled = ws_state_.settled.wait_for(lock, std::chrono::seconds(WS_HANDSHAKE_TIMEOUT_SECS),
                                              [this] { return !ws_state_.connecting; });
    if (!settled) {
        lock.unlock();
        spdlog::error("[NetworkAdapter] WebSocket 握手超时：{}", url);
        ws_close();
        return false;
    }
    if (ws_state_.failed) {
        std::string reason = ws_state_.error;
        lock.unlock();
        spdlog::error("[NetworkAdapter] WebSocket 握手失败：{} / {}", url, reason);
        ws_close();
        return false;
    }

    bool opened = ws_state_.opened;
    lock.unlock();
    return opened;
}

bool ASIONetworkAdapter::ws_send(const WsMessage& message) {
    ix::WebSocket* socket = nullptr;
    {
        std::lock_guard<std::mutex> lock(ws_mutex_);
        socket = ws_.get();
    }
    if (socket == nullptr) {
        spdlog::warn("[NetworkAdapter] WebSocket 未连接，消息被丢弃");
        return false;
    }
    if (socket->getReadyState() != ix::ReadyState::Open) {
        spdlog::warn("[NetworkAdapter] WebSocket 尚未就绪，消息被丢弃");
        return false;
    }

    bool is_binary = (message.type == WsMessageType::BINARY);
    ix::WebSocketSendInfo info = socket->send(message.data, is_binary);
    if (!info.success) {
        spdlog::warn("[NetworkAdapter] WebSocket 发送失败 payload={} wire={}", info.payloadSize,
                     info.wireSize);
    }
    return info.success;
}

bool ASIONetworkAdapter::ws_close() {
    if (on_callback_thread()) {
        // stop() 会 join 当前线程，自 join 是未定义行为；这里什么都不做，交给控制线程。
        spdlog::error("[NetworkAdapter] 不允许在 WebSocket 回调线程内关闭连接");
        return false;
    }

    std::unique_ptr<ix::WebSocket> socket;
    {
        std::lock_guard<std::mutex> lock(ws_mutex_);
        socket = std::move(ws_);
    }
    if (!socket) {
        return true;
    }

    socket->stop();
    {
        std::lock_guard<std::mutex> lock(ws_state_.mutex);
        ws_state_.connecting = false;
        ws_state_.opened = false;
        ws_state_.failed = false;
        ws_state_.thread_id = std::thread::id();
        ws_state_.callback = nullptr;
    }
    spdlog::debug("[NetworkAdapter] WebSocket 连接已关闭");
    return true;
}

void ASIONetworkAdapter::handle_ws_message(const ix::WebSocketMessagePtr& message) {
    if (!message) {
        return;
    }

    WsCallback* callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(ws_state_.mutex);
        ws_state_.thread_id = std::this_thread::get_id();
        callback = ws_state_.callback;
    }
    if (callback == nullptr) {
        return;
    }

    switch (message->type) {
    case ix::WebSocketMessageType::Open: {
        std::lock_guard<std::mutex> lock(ws_state_.mutex);
        ws_state_.opened = true;
        ws_state_.connecting = false;
        ws_state_.settled.notify_all();
    }
        callback->on_connected();
        break;
    case ix::WebSocketMessageType::Message: {
        WsMessage payload;
        payload.type = message->binary ? WsMessageType::BINARY : WsMessageType::TEXT;
        payload.data = message->str;
        callback->on_message(payload);
    } break;
    case ix::WebSocketMessageType::Error: {
        {
            std::lock_guard<std::mutex> lock(ws_state_.mutex);
            ws_state_.failed = true;
            ws_state_.connecting = false;
            ws_state_.error = message->errorInfo.reason;
            ws_state_.settled.notify_all();
        }
        callback->on_error(message->errorInfo.reason);
    } break;
    case ix::WebSocketMessageType::Close:
        callback->on_disconnected();
        break;
    case ix::WebSocketMessageType::Ping:
    case ix::WebSocketMessageType::Pong:
    case ix::WebSocketMessageType::Fragment:
        // 传输层细节，不向上暴露。
        break;
    }
}

} // namespace Prism::Adapter
