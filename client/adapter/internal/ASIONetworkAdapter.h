#pragma once

#include "NetworkAdapter.h"

#include <IXWebSocketMessage.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace ix {
class WebSocket;
} // namespace ix

namespace Prism::Adapter {

/**
 * @class ASIONetworkAdapter
 * @brief 基于 ixwebsocket 的 HTTP / WebSocket 传输实现
 *
 * ## 职责边界
 * 本类只做**字节级传输**：给定 URL 发出 HTTP 请求、把 WS 帧原样交给回调。
 * 信令协议（消息 JSON 编解码、房间状态机、重连退避、业务心跳）属于
 * Business 层 `client-network` 的 `ISignalingClient`，**不在这里实现**——
 * 两层各写一套协议解析必然互相打架。因此本类：
 * - 不做任何 JSON 解析，不区分「业务心跳」与「普通消息」；
 * - 不自动重连（`disableAutomaticReconnection()`），重连时机由上层决定；
 * - 不配置 ping 间隔，RFC 6455 的 Ping/Pong 帧直接被吞掉，不向上暴露。
 *
 * ## 线程模型
 * - HTTP 接口**同步阻塞**。每次调用新建一个 `ix::HttpClient`：ixwebsocket 的
 *   客户端内部是「单 socket + recursive_mutex」，实例不可并发共享，
 *   所以刻意复用会引入跨请求的串行化，这里按请求建实例。
 * - WebSocket 接口**异步**：`ws_connect()` 只阻塞到握手有结果（成功 / 失败 / 超时），
 *   之后 `WsCallback` 的回调由 ixwebsocket 内部线程触发。
 * - `ws_connect()` / `ws_send()` / `ws_close()` / 析构都应来自**同一个控制线程**
 *   （通常是 UI 或服务层的控制线程），类内没有为这几个调用之间加排他。
 *
 * ## 两个硬约束
 * 1. **回调线程内不得调用 `ws_close()`**：ixwebsocket 的 `stop()` 会 join 自己
 *    所在的工作线程，自 join 是未定义行为。检测到这种调用时本类只报错并返回
 *    `false`，不做任何关闭动作，调用方需切到控制线程再关。
 * 2. **析构不得在回调线程内进行**：同上，析构里要 join 工作线程。
 *
 * ## 错误口径
 * `HttpResponse::status_code == 0` 表示请求**没有完成**（URL 非法、连不上、
 * TLS 失败、超时、发送失败等）。此时 `body` 为空，具体原因只进日志：
 * 公共接口没有错误字段，把错误文本塞进 `body` 会让上层无法区分
 * 「服务端返回了这段文本」和「压根没连上」。
 */
class ASIONetworkAdapter : public NetworkAdapter {
  public:
    ASIONetworkAdapter();
    ~ASIONetworkAdapter() override;

    /**
     * @brief 初始化网络子系统（Windows 下即 WSAStartup）
     * @return 成功返回 true；重复调用是安全的，直接返回 true
     */
    bool init() override;

    /**
     * @brief 关闭 WebSocket 连接并释放网络子系统；重复调用是安全的
     */
    void shutdown() override;

    HttpResponse
    http_get(const std::string& url,
             const std::unordered_map<std::string, std::string>& headers = {}) override;
    HttpResponse
    http_post(const std::string& url, const std::string& body,
              const std::unordered_map<std::string, std::string>& headers = {}) override;

    bool ws_connect(const std::string& url, WsCallback* callback) override;
    bool ws_send(const WsMessage& message) override;
    bool ws_close() override;

  private:
    /**
     * @struct WsState
     * @brief WebSocket 连接状态：由控制线程与 ixwebsocket 回调线程共享
     *
     * 单独成结构体是为了让「握手结果」这件事只有一处真相，
     * 而不是散成一堆零散成员各配一把锁。
     */
    struct WsState {
        std::mutex mutex;                /**< 保护本结构体的全部字段 */
        std::condition_variable settled; /**< 握手有结果（成功/失败）时通知等待方 */
        bool opened{false};              /**< 握手是否已成功 */
        bool failed{false};              /**< 握手是否已失败 */
        bool connecting{false};          /**< 控制线程是否仍在等握手结果 */
        std::string error;               /**< 握手失败原因 */
        std::thread::id thread_id{};     /**< 最近一次回调所在的线程，用于识别回调内关闭 */
        WsCallback* callback{nullptr};   /**< 回调接收者，不持有所有权 */
    };

    /**
     * @brief HTTP GET / POST 的共同实现
     * @param url 请求 URL
     * @param verb HTTP 动词，取 `ix::HttpClient::kGet` / `kPost`
     * @param body 请求体，GET 传空串
     * @param headers 自定义请求头
     * @return HttpResponse 响应；`status_code == 0` 表示请求未完成
     */
    HttpResponse send_http(const std::string& url, const std::string& verb, const std::string& body,
                           const std::unordered_map<std::string, std::string>& headers);

    /**
     * @brief ixwebsocket 消息回调的分发入口，运行在 ixwebsocket 的工作线程上
     * @param message 消息，可能为空
     */
    void handle_ws_message(const ix::WebSocketMessagePtr& message);

    /**
     * @brief 判断当前线程是否就是 ixwebsocket 的回调线程
     * @return 是则返回 true
     */
    bool on_callback_thread();

    bool inited_{false};                /**< 网络子系统是否已由本对象初始化 */
    std::mutex ws_mutex_;               /**< 保护 ws_ 指针本身 */
    std::unique_ptr<ix::WebSocket> ws_; /**< 当前连接，未连接时为空 */
    WsState ws_state_;                  /**< 连接状态，见结构体注释 */
};

} // namespace Prism::Adapter
