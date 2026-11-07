#include "ASIONetworkAdapter.h"

#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include <spdlog/spdlog.h>

namespace Prism::Adapter {
namespace {

// ixwebsocket 的默认值是「建连 60s / 传输 1800s」，那是给命令行工具准备的；
// 客户端卡住半小时不可接受，这里收紧到人能等到的时间。
constexpr int HTTP_CONNECT_TIMEOUT_SECS = 10;
constexpr int HTTP_TRANSFER_TIMEOUT_SECS = 60;
constexpr int WS_HANDSHAKE_TIMEOUT_SECS = 10;

/**
 * @brief 把请求头表拼成 ixwebsocket 需要的 `Key: Value\r\n` 文本
 * @param headers 自定义请求头
 * @return 逐行拼接的请求头文本，空表返回空串
 */
std::string to_header_lines(const std::unordered_map<std::string, std::string>& headers) {
    std::string lines;
    for (const auto& item : headers) {
        lines += item.first;
        lines += ": ";
        lines += item.second;
        lines += "\r\n";
    }
    return lines;
}

/**
 * @brief 组装一次 HTTP 请求的参数
 * @param headers 自定义请求头
 * @return 请求参数
 */
[[nodiscard]] ix::HttpRequestArgsPtr
make_request_args(const std::unordered_map<std::string, std::string>& headers) {
    auto args = std::make_shared<ix::HttpRequestArgs>();
    args->extraHeaders = to_header_lines(headers);
    args->connectTimeout = HTTP_CONNECT_TIMEOUT_SECS;
    args->transferTimeout = HTTP_TRANSFER_TIMEOUT_SECS;
    return args;
}

/**
 * @brief 把 ixwebsocket 的响应转成公共接口的响应
 * @param raw ixwebsocket 的响应对象
 * @return 公共响应；请求失败时 `status_code` 保持 0
 */
HttpResponse to_response(const ix::HttpResponsePtr& raw) {
    HttpResponse response;
    if (!raw) {
        spdlog::error("[NetworkAdapter] HTTP request produced no response object");
        return response;
    }

    response.status_code = raw->statusCode;
    response.body = raw->body;
    for (const auto& item : raw->headers) {
        response.headers.emplace(item.first, item.second);
    }

    if (!raw->error.empty()) {
        spdlog::warn("[NetworkAdapter] HTTP request failed: {}", raw->error);
    }
    return response;
}

} // namespace

namespace detail {

/**
 * @struct NetworkImpl
 * @brief ixwebsocket 的客户端状态
 *
 * 单把 `mutex` 保护全部字段：状态转换（握手结果、连接标志、回调指针）都是短临界区，
 * 用一把锁足够，也避免出现「锁顺序」这类只在并发下才暴露的问题。
 */
struct NetworkImpl {
    std::mutex mutex;
    std::shared_ptr<ix::WebSocket> ws; /**< 当前连接，未连接时为空 */
    WsCallback* callback{nullptr};     /**< 上层回调，生命周期由调用方保证 */
    std::thread::id callback_thread;   /**< ixwebsocket 工作线程的 id，用于防止自 join */
    std::condition_variable settled;   /**< 握手有结果（Open/Close/Error）时唤醒 ws_connect */
    bool connecting{false};            /**< 正在等握手结果 */
    bool opened{false};                /**< 连接已建立 */
    bool failed{false};                /**< 握手失败 */
    std::string error;                 /**< 最近一次错误描述 */
    bool net_ready{false};             /**< ix::initNetSystem() 已调用 */

    /**
     * @brief 把 ixwebsocket 的事件转给上层回调
     * @param message ixwebsocket 的消息
     */
    void on_message(const ix::WebSocketMessagePtr& message);
};

void NetworkImpl::on_message(const ix::WebSocketMessagePtr& message) {
    if (!message) {
        return;
    }

    WsCallback* handler = nullptr;
    WsMessage payload;
    std::string error_text;
    bool is_connected = false;
    bool is_closed = false;
    bool is_error = false;
    bool is_message = false;

    {
        std::lock_guard<std::mutex> lock(mutex);
        callback_thread = std::this_thread::get_id();
        handler = callback;

        switch (message->type) {
        case ix::WebSocketMessageType::Open:
            opened = true;
            connecting = false;
            is_connected = true;
            settled.notify_all();
            break;
        case ix::WebSocketMessageType::Close:
            opened = false;
            connecting = false;
            is_closed = true;
            settled.notify_all();
            break;
        case ix::WebSocketMessageType::Error:
            failed = true;
            connecting = false;
            error = message->errorInfo.reason;
            error_text = error;
            is_error = true;
            settled.notify_all();
            break;
        case ix::WebSocketMessageType::Message:
            payload.type = message->binary ? WsMessageType::BINARY : WsMessageType::TEXT;
            payload.data = message->str;
            is_message = true;
            break;
        default:
            // Ping / Pong / Fragment 是传输层细节，不向上暴露。
            return;
        }
    }

    if (handler == nullptr) {
        return;
    }

    // 用户回调一律在锁外调用：回调里调 ws_send() 会再去拿同一把锁。
    if (is_connected) {
        handler->on_connected();
    } else if (is_closed) {
        handler->on_disconnected();
    } else if (is_message) {
        handler->on_message(payload);
    } else if (is_error) {
        handler->on_error(error_text);
    }
}

} // namespace detail

ASIONetworkAdapter::ASIONetworkAdapter() : impl_(std::make_shared<detail::NetworkImpl>()) {}

ASIONetworkAdapter::~ASIONetworkAdapter() {
    shutdown();
}

bool ASIONetworkAdapter::init() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->net_ready) {
        return true;
    }

    // Windows 上要 WSAStartup，其它平台是空实现；与 server/network 的用法保持一致。
    ix::initNetSystem();
    impl_->net_ready = true;
    spdlog::debug("[NetworkAdapter] network subsystem ready");
    return true;
}

void ASIONetworkAdapter::shutdown() {
    ws_close();

    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->net_ready) {
        return;
    }

    ix::uninitNetSystem();
    impl_->net_ready = false;
    spdlog::debug("[NetworkAdapter] network subsystem released");
}

HttpResponse
ASIONetworkAdapter::http_get(const std::string& url,
                             const std::unordered_map<std::string, std::string>& headers) {
    if (url.empty()) {
        spdlog::error("[NetworkAdapter] http_get() called with an empty url");
        return HttpResponse{};
    }
    if (!init()) {
        return HttpResponse{};
    }

    // 每次调用新建一个 HttpClient：它内部是「单 socket + 互斥量」，实例不共享更省心。
    ix::HttpClient client;
    return to_response(client.get(url, make_request_args(headers)));
}

HttpResponse
ASIONetworkAdapter::http_post(const std::string& url, const std::string& body,
                              const std::unordered_map<std::string, std::string>& headers) {
    if (url.empty()) {
        spdlog::error("[NetworkAdapter] http_post() called with an empty url");
        return HttpResponse{};
    }
    if (!init()) {
        return HttpResponse{};
    }

    ix::HttpClient client;
    return to_response(client.post(url, body, make_request_args(headers)));
}

bool ASIONetworkAdapter::ws_connect(const std::string& url, WsCallback* callback) {
    if (url.empty()) {
        spdlog::error("[NetworkAdapter] ws_connect() called with an empty url");
        return false;
    }
    if (callback == nullptr) {
        spdlog::error("[NetworkAdapter] ws_connect() called without a callback");
        return false;
    }
    if (!init()) {
        return false;
    }
    // 一个适配器只维持一条连接：先收掉上一条。
    if (!ws_close()) {
        return false;
    }

    auto socket = std::make_shared<ix::WebSocket>();
    socket->setUrl(url);
    // 重连策略属于 Business 层，这里关掉自动重连，避免和上层的退避逻辑打架。
    socket->disableAutomaticReconnection();
    socket->setHandshakeTimeout(WS_HANDSHAKE_TIMEOUT_SECS);

    std::shared_ptr<detail::NetworkImpl> state = impl_;
    socket->setOnMessageCallback(
        [state](const ix::WebSocketMessagePtr& message) { state->on_message(message); });

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->callback = callback;
        impl_->connecting = true;
        impl_->opened = false;
        impl_->failed = false;
        impl_->error.clear();
        impl_->ws = socket;
    }

    // 不在锁内 start()：新线程的第一件事可能就是回调，没必要让它在锁上排队。
    socket->start();

    // 接口语义是「连上了才返回 true」，所以这里等握手结果，不把 start() 当成功。
    std::unique_lock<std::mutex> lock(impl_->mutex);
    bool settled = impl_->settled.wait_for(lock, std::chrono::seconds(WS_HANDSHAKE_TIMEOUT_SECS),
                                           [this] { return !impl_->connecting; });
    if (!settled) {
        lock.unlock();
        spdlog::error("[NetworkAdapter] WebSocket handshake timed out: {}", url);
        ws_close();
        return false;
    }

    bool opened = impl_->opened;
    std::string reason = impl_->error;
    lock.unlock();

    if (!opened) {
        spdlog::error("[NetworkAdapter] WebSocket handshake failed: {} ({})", url, reason);
        ws_close();
        return false;
    }

    spdlog::info("[NetworkAdapter] WebSocket connected: {}", url);
    return true;
}

bool ASIONetworkAdapter::ws_send(const WsMessage& message) {
    std::shared_ptr<ix::WebSocket> socket;
    bool opened = false;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        socket = impl_->ws;
        opened = impl_->opened;
    }

    if (!socket || !opened) {
        spdlog::warn("[NetworkAdapter] ws_send() called without an open connection");
        return false;
    }

    // shared_ptr 在锁外持有连接对象：即使并发 ws_close() 也不会访问到已释放的内存。
    auto info = socket->send(message.data, message.type == WsMessageType::BINARY);
    if (!info.success) {
        spdlog::warn("[NetworkAdapter] ws_send() failed (payload {} bytes, wire {} bytes)",
                     info.payloadSize, info.wireSize);
    }
    return info.success;
}

bool ASIONetworkAdapter::ws_close() {
    std::shared_ptr<ix::WebSocket> socket;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);

        // stop() 会 join ixwebsocket 的工作线程，在回调线程里调用就是自 join。
        if (impl_->callback_thread != std::thread::id() &&
            impl_->callback_thread == std::this_thread::get_id()) {
            spdlog::error(
                "[NetworkAdapter] ws_close() must not be called from the callback thread");
            return false;
        }

        socket = impl_->ws;
        impl_->ws.reset();
        impl_->callback = nullptr;
        impl_->connecting = false;
        impl_->opened = false;
    }

    // 让还在等握手的 ws_connect() 立刻醒来，不要干等到超时。
    impl_->settled.notify_all();

    if (!socket) {
        return true;
    }

    socket->stop();
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->callback_thread = std::thread::id();
    }

    spdlog::debug("[NetworkAdapter] WebSocket closed");
    return true;
}

} // namespace Prism::Adapter
