# Server 层实现设计（Prism-server）

> 状态：已确认（用户于 2026-10-18 批准「方案OK，进入下一个阶段」）
> 对应任务：`task-current/工作任务.txt` —— Server 三层（Network / Business / Adapter）目前 ⛔ 无实现

## 1. 背景与目标

`server/` 目前只有 4 个 `CMakeLists.txt` 与一个 3 行的 `main.cpp`（`int main() { return 0; }`），三层目录 `include/`/`internal/`/`src/` 全空，三个 `server-adapter` / `server-network` / `server-business` 目标因 `aux_source_directory(src SRC)` 为空而不生成。

本设计目标：把 Server 三层从「⛔ 无实现」推进到「🟡 仅骨架（接口已定义，有可编译的最小实现）」，按 `docs/` 全部规范落地。

## 2. 范围决策（已与用户确认）

| 决策点 | 结论 | 理由 |
|---|---|---|
| 数据库 | **内存存储 + 接口留 MySQL**：Adapter 定义 `DBAdapter` 抽象接口，用 `InMemoryDbAdapter` 落地，MySQL 实现延后 | MySQL 客户端依赖不在 vcpkg.json（落地票 #83，判据是 项目计划书.md §9.3 表结构 + §9.4 EXPLAIN 验收） |
| 单测 | **不含单测接线**：本阶段不写 `test/` 用例，不接 GTest | 恢复测试另立项 #54；覆盖率为期望值非门禁 |
| 传输 | **WebSocket 优先 + HTTP 仅接口**：WebSocket（ixwebsocket）承载实时信令为主路径；HTTP API（asio）只定义接口、不实现 REST 逻辑 | 0005 ADR 网络模型：ASIO 单线程事件循环 + ixwebsocket 信令，服务端无 P2P |

**本阶段不做的（延后项）**：MySQL 实现（#83）、单测接线（#54）、HTTP REST 实现、`IConfigAdapter` / `IFileStorageAdapter`（Adapter 其余接口）、TLS/wss 与鉴权令牌。

## 3. 命名空间与目标布局

**命名空间**：所有 Server 代码挂在 `Prism::Server` 下，按层再分子命名空间，避免与客户端 `Prism::Adapter` / `Prism::Business` 撞名（客户端与 Server 是不同进程、不同构建，但源码层面要区分）。

| 层 | 命名空间 | CMake 目标 | 库形态 |
|---|---|---|---|
| Adapter | `Prism::Server::Adapter` | `server-adapter` | STATIC |
| Business | `Prism::Server::Business` | `server-business` | STATIC |
| Network | `Prism::Server::Network` | `server-network` | STATIC |
| 入口 | `Prism::Server`（main.cpp） | `Prism-server` | EXECUTABLE |

**命名约定**（严格对齐 GLOSSARY.md 与 代码规范 §1）：
- Adapter 层接口**不带 `I` 前缀**：`DBAdapter` / `CryptoAdapter`（对照 `FileAdapter`/`ConfigAdapter`/`NetworkAdapter`）；实现用「平台/方案 + 接口名」：`InMemoryDbAdapter` / `OpenSSLCryptoAdapter`（对照 `SQLiteDBAdapter` / `OpenSSLCryptoAdapter`）。
- Business 层接口**带 `I` 前缀**：`IAuthService` / `IRoomManager` / `IMessageRouter` / `IClientNotifier`；实现 = 接口名去 `I`：`AuthService` / `RoomManager` / `MessageRouter`（对照 `ICommandDispatcher` → `CommandDispatcher`）。
- 工厂函数放 `include/` 下的 `*Factory.h`，返回 `std::unique_ptr<I接口>`（对照 `NetworkFactory.h` / `SyncFactory.h`）。
- 文件名 PascalCase；类型 PascalCase；函数/变量 snake_case；宏/常量 UPPER_CASE；`#pragma once`；多态基类 `virtual ~X() = default;`；Doxygen `@brief`/`@param`/`@return`；函数 ≤100 行；重要逻辑 spdlog。

## 4. 分层设计

### 4.1 Adapter 层（`server/adapter/`）

`Prism::Server::Adapter`，底层，不依赖其他 Server 层，仅依赖第三方库（spdlog + OpenSSL）。

**文件清单**

| 文件 | 内容 |
|---|---|
| `include/DBAdapter.h` | `DBAdapter` 抽象接口 + `UserRecord` / `RoomRecord` / `RoomMemberRecord` 类型（类型与接口同文件，对照客户端 `AccountInfo` 在 `DBAdapter.h`） |
| `include/CryptoAdapter.h` | `CryptoAdapter` 抽象接口 + `HashResult` 类型 |
| `include/AdapterFactory.h` | `create_db_adapter()` / `create_crypto_adapter()` |
| `internal/InMemoryDbAdapter.h` | 内存实现 |
| `internal/OpenSSLCryptoAdapter.h` | OpenSSL 实现 |
| `src/InMemoryDbAdapter.cpp` | 内存实现源 |
| `src/OpenSSLCryptoAdapter.cpp` | OpenSSL 实现源 |
| `src/AdapterFactory.cpp` | 工厂定义 |

**`DBAdapter` 接口**（面向 项目计划书.md §9 表结构 USERS/ROOMS/ROOM_MEMBERS，方法返回 `bool`，查询返回 `std::optional<T>` 或 `std::vector<T>`）：

```cpp
class DBAdapter {
public:
    virtual ~DBAdapter() = default;

    // 用户
    virtual bool insert_user(const UserRecord& u) = 0;
    virtual std::optional<UserRecord> query_user_by_username(const std::string& username) = 0;
    virtual std::optional<UserRecord> query_user_by_id(int64_t id) = 0;

    // 房间
    virtual bool insert_room(const RoomRecord& r) = 0;
    virtual std::optional<RoomRecord> query_room_by_id(const std::string& room_id) = 0;
    virtual std::vector<RoomRecord> query_rooms() = 0;

    // 房间成员
    virtual bool insert_room_member(const RoomMemberRecord& m) = 0;
    virtual bool delete_room_member(const std::string& room_id, int64_t user_id) = 0;
    virtual std::vector<RoomMemberRecord> query_room_members(const std::string& room_id) = 0;
};
```

**类型**（struct 成员内部初始化，默认 `0`，不重载构造函数）：

```cpp
struct UserRecord {
    int64_t id{0};
    std::string username;
    std::string password_hash;
    std::string salt;
};

struct RoomRecord {
    std::string room_id;
    std::string name;
    int64_t owner_id{0};
    int32_t max_members{0};
};

struct RoomMemberRecord {
    std::string room_id;
    int64_t user_id{0};
};
```

**`CryptoAdapter` 接口**（对齐客户端 `CryptoAdapter.h` 的哈希方法，本阶段只做密码哈希，不做文件加解密）：

```cpp
struct HashResult {
    std::string hash;   /**< 哈希值（十六进制） */
    std::string salt;   /**< 随机盐（十六进制） */
};

class CryptoAdapter {
public:
    virtual ~CryptoAdapter() = default;
    virtual HashResult hash_password(const std::string& password) = 0;
    virtual bool verify_password(const std::string& password,
                                 const std::string& hash,
                                 const std::string& salt) = 0;
};
```

- `InMemoryDbAdapter`：用 `std::vector` / `std::map` 持有记录；自增 `id`；`insert_user` 重复用户名返回 false；方法加 `std::mutex` 保护（Server 单线程事件循环，但保留锁以防御）。
- `OpenSSLCryptoAdapter`：`hash_password` 用 SHA-256 + 随机盐（`RAND_bytes`）；`verify_password` 对密码+盐重算哈希做常量时间比较。

### 4.2 Business 层（`server/business/`）

`Prism::Server::Business`，依赖 Adapter 接口（`DBAdapter` / `CryptoAdapter`），不依赖 Network 具体类型。

**文件清单**

| 文件 | 内容 |
|---|---|
| `include/ServerTypes.h` | 共享类型（消息路由的枚举与结构体） |
| `include/IAuthService.h` | 注册/登录/登出 |
| `include/IRoomManager.h` | 创建/加入/离开/列出房间 |
| `include/IMessageRouter.h` | 播放同步 + 聊天消息路由 |
| `include/IClientNotifier.h` | 依赖倒置：Business 定义、Network 实现的推送接口 |
| `include/BusinessFactory.h` | `create_auth_service()` / `create_room_manager()` / `create_message_router()` |
| `internal/AuthService.h` | 实现 |
| `internal/RoomManager.h` | 实现 |
| `internal/MessageRouter.h` | 实现 |
| `src/AuthService.cpp` / `src/RoomManager.cpp` / `src/MessageRouter.cpp` / `src/BusinessFactory.cpp` | 实现源 |

**接口签名**

```cpp
// IAuthService
class IAuthService {
public:
    virtual ~IAuthService() = default;
    /// 注册；用户名已存在返回 false
    virtual bool register_user(const std::string& username, const std::string& password) = 0;
    /// 登录；校验通过返回用户 id，否则 nullopt
    virtual std::optional<int64_t> login(const std::string& username, const std::string& password) = 0;
};

// IRoomManager
class IRoomManager {
public:
    virtual ~IRoomManager() = default;
    virtual bool create_room(const std::string& room_id, const std::string& name, int64_t owner_id) = 0;
    virtual bool join_room(const std::string& room_id, int64_t user_id) = 0;
    virtual bool leave_room(const std::string& room_id, int64_t user_id) = 0;
    virtual std::vector<RoomRecord> list_rooms() = 0;
};

// IMessageRouter（inbound 消息按 type 分发到对应逻辑，再经 IClientNotifier 广播）
class IMessageRouter {
public:
    virtual ~IMessageRouter() = default;
    virtual void route(const std::string& sender_user_id, const std::string& json) = 0;
};

// IClientNotifier（依赖倒置：Network 实现）
class IClientNotifier {
public:
    virtual ~IClientNotifier() = default;
    virtual bool send_to_user(int64_t user_id, const std::string& message) = 0;
    virtual bool broadcast_to_room(const std::string& room_id, const std::string& message) = 0;
};
```

**工厂**（依赖注入，构造函数拿到 `DBAdapter*` / `CryptoAdapter*` / `IClientNotifier*`，不持有所有权）：

```cpp
std::unique_ptr<IAuthService> create_auth_service(DBAdapter* db, CryptoAdapter* crypto);
std::unique_ptr<IRoomManager> create_room_manager(DBAdapter* db, IClientNotifier* notifier);
std::unique_ptr<IMessageRouter> create_message_router(DBAdapter* db, IClientNotifier* notifier);
```

- `AuthService`：`register_user` 先查重再 `hash_password` 落库；`login` 查用户、`verify_password`。
- `RoomManager`：成员变更后经 `IClientNotifier::broadcast_to_room` 通知房间内在线成员。
- `MessageRouter`：解析 `ServerTypes.h` 定义的消息 envelope，把 `playback`（PLAY/PAUSE/SEEK/SPEED，对齐客户端 `PlaybackCommand`）与 `chat` 消息转发给房间内其他成员。

### 4.3 Network 层（`server/network/`）

`Prism::Server::Network`，依赖 Business 接口（`IAuthService` / `IRoomManager` / `IMessageRouter` / `IClientNotifier`），实现 `IClientNotifier`。

**文件清单**

| 文件 | 内容 |
|---|---|
| `include/ISignalingServer.h` | WebSocket 服务端抽象接口 |
| `include/IHttpApi.h` | HTTP API 抽象接口（**仅接口声明，无实现**，见下注） |
| `include/NetworkTypes.h` | 连接/会话相关类型 |
| `include/NetworkFactory.h` | `create_signaling_server(...)` |
| `internal/IxWebSocketServer.h` | ixwebsocket 实现 |
| `internal/SessionManager.h` | 会话管理（user_id ↔ 连接映射），实现 `IClientNotifier` |
| `src/IxWebSocketServer.cpp` / `src/SessionManager.cpp` / `src/NetworkFactory.cpp` | 实现源 |

**接口签名**

> `IHttpApi.h` 是「接口先行」（代码规范 §5.2）的声明：只写接口，**不建 internal 桩、不建 src、不接线、不参与构建**。不为它创建 0 字节占位文件（2026-10-11 已清理这类占位文件，不做回归）。

```cpp
// ISignalingServer
class ISignalingServer {
public:
    virtual ~ISignalingServer() = default;
    /// 暴露内部 IClientNotifier，供组装根注入 business 服务
    virtual IClientNotifier* notifier() = 0;
    /// 把 business 接口 set 回网络层，接收入站消息
    virtual void set_handlers(IAuthService* auth,
                              IRoomManager* rooms,
                              IMessageRouter* router) = 0;
    virtual bool start(const std::string& host, uint16_t port) = 0;
    virtual void stop() = 0;
};

// NetworkFactory（无参构造，先建服务端拿到 notifier，再注入 business）
std::unique_ptr<ISignalingServer> create_signaling_server();
```

- `SessionManager`：维护 `user_id → 连接` 与 `room_id → 成员集合` 的映射，实现 `IClientNotifier::send_to_user` / `broadcast_to_room`；客户端登录成功后再绑定 user_id（登录前连接是「匿名」态）。
- `IxWebSocketServer`：用 ixwebsocket 的 `ix::WebSocketServer`，单线程事件循环（对齐 0005 ADR）；内部持有 `SessionManager`，经 `notifier()` 暴露；收到消息反序列化 JSON、按 `type` 路由：`register`/`login` → `IAuthService`，`create_room`/`join_room`/`leave_room` → `IRoomManager`，其余 → `IMessageRouter::route`。
- 消息编解码用 `nlohmann_json`；连接断开时清理 `SessionManager` 的会话。

## 5. 依赖关系与依赖倒置

```
main.cpp（组装根）
  ├─ AdapterFactory  → InMemoryDbAdapter / OpenSSLCryptoAdapter
  ├─ NetworkFactory  → IxWebSocketServer（内部持有 SessionManager）
  └─ BusinessFactory → AuthService / RoomManager / MessageRouter

依赖方向（只依赖接口，不依赖实现）：
  server-network   →  server-business（IAuthService/IRoomManager/IMessageRouter/IClientNotifier）
  server-business  →  server-adapter（DBAdapter/CryptoAdapter）
  server-adapter   →  无（仅第三方）
```

**循环依赖的解决**：Business 需要「向客户端推送」能力，但推送落在 Network。为避免 Business → Network 的依赖，让 Business 定义 `IClientNotifier`（自己的接口），Network 的 `SessionManager` 实现它。于是 Business 只依赖自己定义的接口，Network 依赖 Business 接口 —— 类型层面无环、CMake 链接层面无环（Business 不 link Network）。

## 6. JSON 信令协议（WebSocket）

入站消息统一 envelope（客户端 → 服务端）：

```json
{ "type": "login", "username": "...", "password": "..." }
{ "type": "register", "username": "...", "password": "..." }
{ "type": "create_room", "room_id": "...", "name": "..." }
{ "type": "join_room", "room_id": "..." }
{ "type": "leave_room", "room_id": "..." }
{ "type": "playback", "room_id": "...", "command": "play|pause|seek|speed", "value": 0.0 }
{ "type": "chat", "room_id": "...", "text": "..." }
```

出站响应/推送统一 envelope（服务端 → 客户端）：

```json
{ "type": "ack", "ok": true, "for": "login" }
{ "type": "error", "code": 400, "message": "..." }
{ "type": "playback", "room_id": "...", "sender": 0, "command": "...", "value": 0.0 }
{ "type": "chat", "room_id": "...", "sender": 0, "text": "..." }
{ "type": "room_update", "room_id": "...", "member_count": 1 }
```

`type` 字符串常量集中定义在 `ServerTypes.h` / `NetworkTypes.h`（UPPER_CASE 或 `constexpr`），避免散落字面量。

## 7. CMake 变更

- **server/adapter/CMakeLists.txt**：`add_library` 显式 `STATIC`；新增 `find_package(spdlog REQUIRED)`、`find_package(OpenSSL REQUIRED)`；link `spdlog::spdlog_header_only` + `OpenSSL::Crypto`（PRIVATE）。
- **server/business/CMakeLists.txt**：`add_library` 显式 `STATIC`；新增 `find_package(spdlog REQUIRED)`；link `$<$<TARGET_EXISTS:server-adapter>:server-adapter>`（PUBLIC，因为 `BusinessFactory.h` 暴露 `DBAdapter*`）+ `spdlog::spdlog_header_only`（PRIVATE）。
- **server/network/CMakeLists.txt**：`add_library` 显式 `STATIC`；保留 `find_package(asio REQUIRED)`、`find_package(nlohmann_json REQUIRED)`，新增 `find_package(ixwebsocket REQUIRED)`、`find_package(spdlog REQUIRED)`；link `$<$<TARGET_EXISTS:server-business>:server-business>`（PUBLIC）+ `asio::asio` / `ixwebsocket::ixwebsocket` / `nlohmann_json` / `spdlog::spdlog_header_only`（PRIVATE）。
- **server/CMakeLists.txt**：无需改链接结构（已用 `$<$<TARGET_EXISTS:...>:...>`），补一行 `find_package(spdlog REQUIRED)` 供 main.cpp 用（若 main 不直接打日志可不加）。
- 所有目标继续沿用 `aux_source_directory(src SRC)` + `if(SRC)` 门控，保证「无源文件时不生成目标」这一既有语义。

## 8. main.cpp 组装根

```cpp
#include "AdapterFactory.h"
#include "BusinessFactory.h"
#include "NetworkFactory.h"

int main() {
    // 1. Adapter
    auto db = Prism::Server::Adapter::create_db_adapter();
    auto crypto = Prism::Server::Adapter::create_crypto_adapter();

    // 2. Network（先建 server，拿到 notifier）
    auto server = Prism::Server::Network::create_signaling_server();
    auto notifier = server->notifier();

    // 3. Business（注入 adapter + notifier）
    auto auth = Prism::Server::Business::create_auth_service(db.get(), crypto.get());
    auto rooms = Prism::Server::Business::create_room_manager(db.get(), notifier);
    auto router = Prism::Server::Business::create_message_router(db.get(), notifier);

    // 4. 把 business 接口 set 回网络层，启动
    server->set_handlers(auth.get(), rooms.get(), router.get());
    server->start("0.0.0.0", 8080);
    return 0;
}
```

## 9. 验证方式

1. **编译**：Windows 与 Linux 双预设均能构建出 `server-adapter` / `server-business` / `server-network` / `Prism-server` 四个目标（`aux_source_directory` 找到 `.cpp` 后目标由不生成转为生成）。
2. **格式**：`clang-format --dry-run --Werror`（`.clang-format`，列宽 100）通过 —— 即 pre-commit 门禁。
3. **静态检查**：`pre-push` 三条 clang-tidy 规则（`readability-identifier-naming`、`readability-function-size` ≤100、`cppcoreguidelines-virtual-class-destructor`）零命中。
4. **冒烟**：`Prism-server` 启动后，用 `ixwebsocket` 自带客户端或 `websocat` 连 `ws://127.0.0.1:8080`，依次发 register → login → create_room → join_room → playback，观察 ack / 广播回包与 spdlog 日志。

## 10. 延后项（不在本阶段）

| 项 | 依据 |
|---|---|
| MySQL `DBAdapter` 实现 | vcpkg.json 无 mysql 依赖，落地票 #83 |
| 单测接线（GTest / `test/`） | #54，覆盖率期望值非门禁 |
| HTTP REST 实现 | `IHttpApi` 仅留接口占位 |
| `IConfigAdapter` / `IFileStorageAdapter` | Adapter 其余接口，本阶段聚焦 DB + Crypto |
| TLS/wss、鉴权令牌、心跳 | 0005 ADR 后续阶段 |
