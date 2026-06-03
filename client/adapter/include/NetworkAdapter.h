#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace Prism::Adapter {

/**
 * @struct HttpResponse
 * @brief HTTP 响应
 */
struct HttpResponse {
    int status_code{0};                                           /**< HTTP 状态码 */
    std::string body;                                             /**< 响应体 */
    std::unordered_map<std::string, std::string> headers;         /**< 响应头 */
};

/**
 * @enum WsMessageType
 * @brief WebSocket 消息类型
 */
enum class WsMessageType {
    TEXT,    /**< 文本消息 */
    BINARY   /**< 二进制消息 */
};

/**
 * @struct WsMessage
 * @brief WebSocket 消息
 */
struct WsMessage {
    WsMessageType type;          /**< 消息类型 */
    std::string data;            /**< 消息数据 */
};

/**
 * @class WsCallback
 * @brief WebSocket 事件回调接口，由上层实现
 */
class WsCallback {
public:
    virtual ~WsCallback() = default;

    /**
     * @brief 连接建立回调
     */
    virtual void on_connected() = 0;

    /**
     * @brief 连接断开回调
     */
    virtual void on_disconnected() = 0;

    /**
     * @brief 收到消息回调
     * @param message WebSocket 消息
     */
    virtual void on_message(const WsMessage& message) = 0;

    /**
     * @brief 发生错误回调
     * @param error 错误描述
     */
    virtual void on_error(const std::string& error) = 0;
};

/**
 * @class NetworkAdapter
 * @brief 网络通信抽象接口，封装 HTTP 请求与 WebSocket 通信
 */
class NetworkAdapter {
public:
    virtual ~NetworkAdapter() = default;

    /**
     * @brief 初始化网络通信
     * @return 成功返回 true
     */
    virtual bool init() = 0;

    /**
     * @brief 关闭网络通信
     */
    virtual void shutdown() = 0;

    // ==================== HTTP ====================

    /**
     * @brief 发送 HTTP GET 请求
     * @param url 请求 URL
     * @param headers 自定义请求头
     * @return HttpResponse 响应
     */
    virtual HttpResponse http_get(const std::string& url,
                                  const std::unordered_map<std::string, std::string>& headers = {}) = 0;

    /**
     * @brief 发送 HTTP POST 请求
     * @param url 请求 URL
     * @param body 请求体
     * @param headers 自定义请求头
     * @return HttpResponse 响应
     */
    virtual HttpResponse http_post(const std::string& url,
                                   const std::string& body,
                                   const std::unordered_map<std::string, std::string>& headers = {}) = 0;

    // ==================== WebSocket ====================

    /**
     * @brief 建立 WebSocket 连接
     * @param url WebSocket 服务端地址
     * @param callback 事件回调（生命周期由调用者管理）
     * @return 成功返回 true
     */
    virtual bool ws_connect(const std::string& url, WsCallback* callback) = 0;

    /**
     * @brief 发送 WebSocket 消息
     * @param message 消息
     * @return 成功返回 true
     */
    virtual bool ws_send(const WsMessage& message) = 0;

    /**
     * @brief 关闭 WebSocket 连接
     * @return 成功返回 true
     */
    virtual bool ws_close() = 0;
};

} // namespace Prism::Adapter
