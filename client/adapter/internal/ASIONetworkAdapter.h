#pragma once

#include <memory>

#include "NetworkAdapter.h"

namespace Prism::Adapter {

namespace detail {

/** @brief ixwebsocket 相关状态，定义在 `ASIONetworkAdapter.cpp` 里，避免头文件外泄第三方类型 */
struct NetworkImpl;

} // namespace detail

/**
 * @class ASIONetworkAdapter
 * @brief 基于 ASIO + ixwebsocket 的网络通信实现
 *
 * **边界**：本类只做字节级传输 —— HTTP 同步请求、WebSocket 异步收发。信令协议
 * （消息编解码、重连退避、心跳、房间/账号流程）属于 Business 层 `client-network` 的
 * `ISignalingClient`，这里不重复实现。基于同一理由，ixwebsocket 自带的自动重连被显式
 * 关闭：重连是策略，由上层决定。
 *
 * **为什么不单独持有 ASIO 的 `io_context`**：ixwebsocket 内部已经带了一条 ASIO 事件
 * 循环并只暴露给它自己；再建一个只会多出一个 Reactor 线程，没有任何调用方。
 *
 * **线程模型**：`http_get` / `http_post` 在调用线程上阻塞；`ws_*` 的回调在 ixwebsocket
 * 的工作线程上执行，因此实现内部对回调指针加了锁。`ws_close()` 会 join 该工作线程，
 * 在回调线程里调用它会自 join（未定义行为），实现检测到这种调用只记错误并返回 false。
 */
class ASIONetworkAdapter : public NetworkAdapter {
  public:
    ASIONetworkAdapter();
    ~ASIONetworkAdapter() override;

    // 持有连接与工作线程，拷贝没有意义。
    ASIONetworkAdapter(const ASIONetworkAdapter&) = delete;
    ASIONetworkAdapter& operator=(const ASIONetworkAdapter&) = delete;

    bool init() override;
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
    std::shared_ptr<detail::NetworkImpl> impl_; /**< 与回调共享所有权，回调不会碰到已释放的对象 */
};

} // namespace Prism::Adapter
