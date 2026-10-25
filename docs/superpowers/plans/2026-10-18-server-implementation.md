# Server 层实现计划（Prism-server）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `server/` 三层（Adapter / Business / Network）从「⛔ 无实现」推进到「🟡 仅骨架（接口已定义 + 可编译的最小实现）」，跑通 WebSocket 信令主路径。

**Architecture:** 三层静态库 + 一个组装根。Adapter 提供 `DBAdapter`（内存实现）+ `CryptoAdapter`（OpenSSL）；Business 提供 `IAuthService` / `IRoomManager` / `IMessageRouter`，并通过自己定义的 `IClientNotifier` 接口做依赖倒置；Network 用 ixwebsocket 实现 `ISignalingServer` 与 `IClientNotifier`（SessionManager）。`main.cpp` 组装三者。

**Tech Stack:** C++17、CMake + Ninja、ixwebsocket（WebSocket）、OpenSSL（SHA-256）、nlohmann-json、spdlog。

**Spec:** `docs/superpowers/specs/2026-10-18-server-implementation-design.md`

---

## 重要约定（先读）

- **提交（commit）步骤本阶段一律跳过**：用户已明确「暂不提交到 github 仓库」，所有改动留在工作区。计划里不写 commit 步骤；待用户说「提交」再统一提交。
- **工作区既有改动不要碰**：`.gitignore`、`ui/analysis_options.yaml`、`ui/pubspec.lock`、`vcpkg.json` 四个文件有未提交改动。其中 `vcpkg.json` 是在 server feature 里加了 `mysql`（属 #83，非本阶段）。**不要 `git add` 它们，也不要 `git checkout` 它们**。
- **验证方式**：本阶段不做 GTest 单测（#54 延后），每个任务的验证 = `clang-format --dry-run --Werror`（格式）+ 目标编译通过（`cmake --build`）。命名/函数长度/虚析构三条 clang-tidy 规则由 pre-push 承载，实现完成后再跑一次 tidy 复核。
- **命名**（GLOSSARY 对齐）：Adapter 接口不带 `I`（`DBAdapter`/`CryptoAdapter`），Business 接口带 `I`（`IAuthService` 等），实现 = 接口名去 `I`（`AuthService`）；命名空间 `Prism::Server::{Adapter,Business,Network}`；常量全大写（`inline constexpr const char*` 用 `MSG_LOGIN` 这种，避免 `kMsgLogin` 被 `readability-identifier-naming` 的 `ConstantCase` 判定命中）。

## 构建命令（Windows 本机）

```bash
# 首次配置（一次性，可能较慢；见下方 mysql 注）
cmake --preset windows-clang-x64-Debug
# 编译单个目标（后续增量很快）
cmake --build --preset windows-clang-x64-Debug --target server-adapter
cmake --build --preset windows-clang-x64-Debug --target server-business
cmake --build --preset windows-clang-x64-Debug --target server-network
cmake --build --preset windows-clang-x64-Debug --target Prism-server
# 格式检查（clang-format 23.1，二进制在 $CLANG_ROOT/clang-format）
clang-format --dry-run --Werror <files...>
```

> **mysql 注**：`vcpkg.json` 当前工作区含未提交的 `mysql` 依赖。首次 `cmake --preset` 会触发 vcpkg 重装并尝试拉取 mysql（大、慢，且与 #83 无关）。若本机已有可用的构建树（如根 `build/`，已装齐 ixwebsocket/openssl/nlohmann/spdlog 且无 mysql），可直接在该树上增量编译。若配置被 mysql 卡住，暂停并知会用户，**不要**自行改 vcpkg.json。

## 文件结构

```
server/
├── CMakeLists.txt                       [修改] 加 find_package(spdlog) + link
├── main.cpp                             [重写] 组装根
├── adapter/
│   ├── CMakeLists.txt                   [修改] STATIC + OpenSSL + spdlog
│   ├── include/
│   │   ├── DBAdapter.h                  [新建] DBAdapter 接口 + UserRecord/RoomRecord/RoomMemberRecord
│   │   ├── CryptoAdapter.h              [新建] CryptoAdapter 接口 + HashResult
│   │   └── AdapterFactory.h             [新建] create_db_adapter/create_crypto_adapter
│   ├── internal/
│   │   ├── InMemoryDbAdapter.h          [新建] 内存实现
│   │   └── OpenSSLCryptoAdapter.h       [新建] OpenSSL 实现
│   └── src/
│       ├── InMemoryDbAdapter.cpp        [新建]
│       ├── OpenSSLCryptoAdapter.cpp     [新建]
│       └── AdapterFactory.cpp           [新建]
├── business/
│   ├── CMakeLists.txt                   [修改] STATIC + nlohmann_json + link adapter
│   ├── include/
│   │   ├── ServerTypes.h                [新建] 消息 type 常量
│   │   ├── IAuthService.h               [新建]
│   │   ├── IRoomManager.h               [新建]
│   │   ├── IMessageRouter.h             [新建]
│   │   ├── IClientNotifier.h            [新建] 依赖倒置接口
│   │   └── BusinessFactory.h            [新建]
│   ├── internal/
│   │   ├── AuthService.h                [新建]
│   │   ├── RoomManager.h                [新建]
│   │   └── MessageRouter.h              [新建]
│   └── src/
│       ├── AuthService.cpp              [新建]
│       ├── RoomManager.cpp              [新建]
│       ├── MessageRouter.cpp            [新建]
│       └── BusinessFactory.cpp          [新建]
└── network/
    ├── CMakeLists.txt                   [修改] STATIC + ixwebsocket + spdlog + link business
    ├── include/
    │   ├── ISignalingServer.h           [新建]
    │   ├── IHttpApi.h                   [新建] 仅接口声明（§5.2 接口先行）
    │   └── NetworkFactory.h             [新建]
    ├── internal/
    │   ├── SessionManager.h             [新建] 实现 IClientNotifier
    │   └── IxWebSocketServer.h          [新建]
    └── src/
        ├── SessionManager.cpp           [新建]
        ├── IxWebSocketServer.cpp        [新建]
        └── NetworkFactory.cpp           [新建]
```

> 相对 spec 的偏差：spec 曾列 `network/include/NetworkTypes.h`，本计划**不建**——网络层没有需要公开的共享类型，空文件会退回占位文件反模式（YAGNI）。`ISignalingServer` 较 spec 多一个 `wait()`（主线程阻塞需要）。

---

## Task 1: Adapter 类型与接口

**Files:**
- Create: `server/adapter/include/DBAdapter.h`
- Create: `server/adapter/include/CryptoAdapter.h`
- Create: `server/adapter/include/AdapterFactory.h`

- [ ] **Step 1.1: 写 `DBAdapter.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Prism::Server::Adapter {

/**
 * @struct UserRecord
 * @brief 用户记录
 */
struct UserRecord {
    int64_t id{0};            /**< 用户 ID */
    std::string username;     /**< 用户名 */
    std::string password_hash; /**< 密码哈希（十六进制） */
    std::string salt;         /**< 密码盐（十六进制） */
};

/**
 * @struct RoomRecord
 * @brief 房间记录
 */
struct RoomRecord {
    std::string room_id;      /**< 房间 ID */
    std::string name;         /**< 房间名 */
    int64_t owner_id{0};      /**< 房主用户 ID */
    int32_t max_members{0};   /**< 最大成员数 */
};

/**
 * @struct RoomMemberRecord
 * @brief 房间成员记录
 */
struct RoomMemberRecord {
    std::string room_id;      /**< 房间 ID */
    int64_t user_id{0};       /**< 用户 ID */
};

/**
 * @class DBAdapter
 * @brief 数据库操作抽象接口（当前内存实现，MySQL 实现延后到 #83）
 */
class DBAdapter {
public:
    virtual ~DBAdapter() = default;

    /**
     * @brief 插入用户
     * @param user 用户记录
     * @return 成功返回 true，用户名重复返回 false
     */
    virtual bool insert_user(const UserRecord& user) = 0;

    /**
     * @brief 按用户名查询用户
     * @param username 用户名
     * @return 找到返回 UserRecord，否则 nullopt
     */
    virtual std::optional<UserRecord> query_user_by_username(const std::string& username) = 0;

    /**
     * @brief 按 ID 查询用户
     * @param id 用户 ID
     * @return 找到返回 UserRecord，否则 nullopt
     */
    virtual std::optional<UserRecord> query_user_by_id(int64_t id) = 0;

    /**
     * @brief 插入房间
     * @param room 房间记录
     * @return 成功返回 true，房间 ID 重复返回 false
     */
    virtual bool insert_room(const RoomRecord& room) = 0;

    /**
     * @brief 按 ID 查询房间
     * @param room_id 房间 ID
     * @return 找到返回 RoomRecord，否则 nullopt
     */
    virtual std::optional<RoomRecord> query_room_by_id(const std::string& room_id) = 0;

    /**
     * @brief 查询全部房间
     * @return 房间列表
     */
    virtual std::vector<RoomRecord> query_rooms() = 0;

    /**
     * @brief 插入房间成员
     * @param member 房间成员记录
     * @return 成功返回 true，已是成员返回 false
     */
    virtual bool insert_room_member(const RoomMemberRecord& member) = 0;

    /**
     * @brief 删除房间成员
     * @param room_id 房间 ID
     * @param user_id 用户 ID
     * @return 成功返回 true
     */
    virtual bool delete_room_member(const std::string& room_id, int64_t user_id) = 0;

    /**
     * @brief 查询房间全部成员
     * @param room_id 房间 ID
     * @return 成员列表
     */
    virtual std::vector<RoomMemberRecord> query_room_members(const std::string& room_id) = 0;
};

} // namespace Prism::Server::Adapter
```

- [ ] **Step 1.2: 写 `CryptoAdapter.h`**

```cpp
#pragma once

#include <string>

namespace Prism::Server::Adapter {

/**
 * @struct HashResult
 * @brief 密码哈希结果
 */
struct HashResult {
    std::string hash;  /**< 哈希值（十六进制） */
    std::string salt;  /**< 随机盐（十六进制） */
};

/**
 * @class CryptoAdapter
 * @brief 密码学操作抽象接口（密码哈希）
 */
class CryptoAdapter {
public:
    virtual ~CryptoAdapter() = default;

    /**
     * @brief 对明文密码做哈希（SHA-256 + 随机盐）
     * @param password 明文密码
     * @return HashResult 哈希与盐值
     */
    virtual HashResult hash_password(const std::string& password) = 0;

    /**
     * @brief 校验密码是否匹配
     * @param password 明文密码
     * @param hash 已存哈希（十六进制）
     * @param salt 已存盐值（十六进制）
     * @return 匹配返回 true
     */
    virtual bool verify_password(const std::string& password,
                                 const std::string& hash,
                                 const std::string& salt) = 0;
};

} // namespace Prism::Server::Adapter
```

- [ ] **Step 1.3: 写 `AdapterFactory.h`**

```cpp
#pragma once

#include <memory>

namespace Prism::Server::Adapter {

class DBAdapter;
class CryptoAdapter;

/**
 * @brief 创建数据库适配器实例（内存实现）
 * @return DBAdapter 实例
 */
std::unique_ptr<DBAdapter> create_db_adapter();

/**
 * @brief 创建密码学适配器实例（OpenSSL 实现）
 * @return CryptoAdapter 实例
 */
std::unique_ptr<CryptoAdapter> create_crypto_adapter();

} // namespace Prism::Server::Adapter
```

- [ ] **Step 1.4: 格式检查**

Run: `clang-format --dry-run --Werror server/adapter/include/DBAdapter.h server/adapter/include/CryptoAdapter.h server/adapter/include/AdapterFactory.h`
Expected: 无输出，退出码 0

---

## Task 2: Adapter 内存实现

**Files:**
- Create: `server/adapter/internal/InMemoryDbAdapter.h`
- Create: `server/adapter/internal/OpenSSLCryptoAdapter.h`
- Create: `server/adapter/src/InMemoryDbAdapter.cpp`
- Create: `server/adapter/src/OpenSSLCryptoAdapter.cpp`

- [ ] **Step 2.1: 写 `InMemoryDbAdapter.h`**

```cpp
#pragma once

#include "DBAdapter.h"

#include <map>
#include <mutex>
#include <vector>

namespace Prism::Server::Adapter {

/**
 * @class InMemoryDbAdapter
 * @brief 内存数据库实现
 */
class InMemoryDbAdapter : public DBAdapter {
public:
    InMemoryDbAdapter() = default;
    ~InMemoryDbAdapter() override = default;

    bool insert_user(const UserRecord& user) override;
    std::optional<UserRecord> query_user_by_username(const std::string& username) override;
    std::optional<UserRecord> query_user_by_id(int64_t id) override;

    bool insert_room(const RoomRecord& room) override;
    std::optional<RoomRecord> query_room_by_id(const std::string& room_id) override;
    std::vector<RoomRecord> query_rooms() override;

    bool insert_room_member(const RoomMemberRecord& member) override;
    bool delete_room_member(const std::string& room_id, int64_t user_id) override;
    std::vector<RoomMemberRecord> query_room_members(const std::string& room_id) override;

private:
    std::mutex mutex_;
    std::map<int64_t, UserRecord> users_;
    int64_t next_user_id_{1};
    std::map<std::string, RoomRecord> rooms_;
    std::map<std::string, std::vector<RoomMemberRecord>> room_members_;
};

} // namespace Prism::Server::Adapter
```

- [ ] **Step 2.2: 写 `InMemoryDbAdapter.cpp`**

```cpp
#include "InMemoryDbAdapter.h"

#include <spdlog/spdlog.h>

namespace Prism::Server::Adapter {

bool InMemoryDbAdapter::insert_user(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : users_) {
        if (entry.second.username == user.username) {
            spdlog::warn("insert_user: username '{}' already exists", user.username);
            return false;
        }
    }
    UserRecord record = user;
    record.id = next_user_id_++;
    users_.emplace(record.id, record);
    return true;
}

std::optional<UserRecord> InMemoryDbAdapter::query_user_by_username(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : users_) {
        if (entry.second.username == username) {
            return entry.second;
        }
    }
    return std::nullopt;
}

std::optional<UserRecord> InMemoryDbAdapter::query_user_by_id(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(id);
    if (it == users_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool InMemoryDbAdapter::insert_room(const RoomRecord& room) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (rooms_.count(room.room_id) > 0) {
        return false;
    }
    rooms_.emplace(room.room_id, room);
    return true;
}

std::optional<RoomRecord> InMemoryDbAdapter::query_room_by_id(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<RoomRecord> InMemoryDbAdapter::query_rooms() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RoomRecord> result;
    result.reserve(rooms_.size());
    for (const auto& entry : rooms_) {
        result.push_back(entry.second);
    }
    return result;
}

bool InMemoryDbAdapter::insert_room_member(const RoomMemberRecord& member) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& members = room_members_[member.room_id];
    for (const auto& m : members) {
        if (m.user_id == member.user_id) {
            return false;
        }
    }
    members.push_back(member);
    return true;
}

bool InMemoryDbAdapter::delete_room_member(const std::string& room_id, int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = room_members_.find(room_id);
    if (it == room_members_.end()) {
        return false;
    }
    auto& members = it->second;
    for (auto member_it = members.begin(); member_it != members.end(); ++member_it) {
        if (member_it->user_id == user_id) {
            members.erase(member_it);
            return true;
        }
    }
    return false;
}

std::vector<RoomMemberRecord> InMemoryDbAdapter::query_room_members(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = room_members_.find(room_id);
    if (it == room_members_.end()) {
        return {};
    }
    return it->second;
}

} // namespace Prism::Server::Adapter
```

- [ ] **Step 2.3: 写 `OpenSSLCryptoAdapter.h`**

```cpp
#pragma once

#include "CryptoAdapter.h"

namespace Prism::Server::Adapter {

/**
 * @class OpenSSLCryptoAdapter
 * @brief 基于 OpenSSL 的密码哈希实现（SHA-256 + 随机盐）
 */
class OpenSSLCryptoAdapter : public CryptoAdapter {
public:
    OpenSSLCryptoAdapter() = default;
    ~OpenSSLCryptoAdapter() override = default;

    HashResult hash_password(const std::string& password) override;
    bool verify_password(const std::string& password,
                         const std::string& hash,
                         const std::string& salt) override;
};

} // namespace Prism::Server::Adapter
```

- [ ] **Step 2.4: 写 `OpenSSLCryptoAdapter.cpp`**

```cpp
#include "OpenSSLCryptoAdapter.h"

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <string>
#include <vector>

namespace Prism::Server::Adapter {

namespace {

constexpr int SALT_SIZE = 16;

std::string to_hex(const unsigned char* data, size_t size) {
    static const char HEX[] = "0123456789abcdef";
    std::string result;
    result.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) {
        result.push_back(HEX[data[i] >> 4]);
        result.push_back(HEX[data[i] & 0x0f]);
    }
    return result;
}

std::vector<unsigned char> from_hex(const std::string& hex) {
    auto nibble = [](char c) -> unsigned char {
        if (c >= '0' && c <= '9') {
            return static_cast<unsigned char>(c - '0');
        }
        if (c >= 'a' && c <= 'f') {
            return static_cast<unsigned char>(c - 'a' + 10);
        }
        if (c >= 'A' && c <= 'F') {
            return static_cast<unsigned char>(c - 'A' + 10);
        }
        return 0;
    };
    std::vector<unsigned char> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        result.push_back(static_cast<unsigned char>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
    }
    return result;
}

} // namespace

HashResult OpenSSLCryptoAdapter::hash_password(const std::string& password) {
    unsigned char salt[SALT_SIZE] = {};
    if (RAND_bytes(salt, SALT_SIZE) != 1) {
        return {};
    }

    std::string salted(reinterpret_cast<char*>(salt), SALT_SIZE);
    salted += password;

    unsigned char digest[SHA256_DIGEST_LENGTH] = {};
    SHA256(reinterpret_cast<const unsigned char*>(salted.data()), salted.size(), digest);

    HashResult result;
    result.salt = to_hex(salt, SALT_SIZE);
    result.hash = to_hex(digest, SHA256_DIGEST_LENGTH);
    return result;
}

bool OpenSSLCryptoAdapter::verify_password(const std::string& password,
                                           const std::string& hash,
                                           const std::string& salt) {
    std::vector<unsigned char> salt_bytes = from_hex(salt);
    std::string salted(reinterpret_cast<char*>(salt_bytes.data()), salt_bytes.size());
    salted += password;

    unsigned char digest[SHA256_DIGEST_LENGTH] = {};
    SHA256(reinterpret_cast<const unsigned char*>(salted.data()), salted.size(), digest);

    std::string computed = to_hex(digest, SHA256_DIGEST_LENGTH);
    if (computed.size() != hash.size()) {
        return false;
    }
    return CRYPTO_memcmp(computed.data(), hash.data(), computed.size()) == 0;
}

} // namespace Prism::Server::Adapter
```

- [ ] **Step 2.5: 格式检查**

Run: `clang-format --dry-run --Werror server/adapter/internal/InMemoryDbAdapter.h server/adapter/internal/OpenSSLCryptoAdapter.h server/adapter/src/InMemoryDbAdapter.cpp server/adapter/src/OpenSSLCryptoAdapter.cpp`
Expected: 无输出，退出码 0

---

## Task 3: Adapter 工厂 + CMake

**Files:**
- Create: `server/adapter/src/AdapterFactory.cpp`
- Modify: `server/adapter/CMakeLists.txt`

- [ ] **Step 3.1: 写 `AdapterFactory.cpp`**

```cpp
#include "AdapterFactory.h"

#include "InMemoryDbAdapter.h"
#include "OpenSSLCryptoAdapter.h"

namespace Prism::Server::Adapter {

std::unique_ptr<DBAdapter> create_db_adapter() {
    return std::make_unique<InMemoryDbAdapter>();
}

std::unique_ptr<CryptoAdapter> create_crypto_adapter() {
    return std::make_unique<OpenSSLCryptoAdapter>();
}

} // namespace Prism::Server::Adapter
```

- [ ] **Step 3.2: 改 `server/adapter/CMakeLists.txt`**

把整份文件替换为：

```cmake
project(server-adapter)

message(STATUS "${PROJECT_NAME} is building")

find_package(spdlog REQUIRED)
find_package(OpenSSL REQUIRED)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)

if(SRC)
    message(STATUS "The ${PROJECT_NAME} of source file include:")
    foreach(file ${SRC})
        get_filename_component(name ${file} NAME)
        message(STATUS "${name}")
    endforeach()
    message(STATUS "")

    add_library(${PROJECT_NAME} STATIC ${SRC})
    target_include_directories(${PROJECT_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/internal
    )

    target_link_libraries(${PROJECT_NAME}
    PRIVATE
        spdlog::spdlog_header_only
        OpenSSL::Crypto
    )
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
```

- [ ] **Step 3.3: 编译 `server-adapter`**

Run: `cmake --build --preset windows-clang-x64-Debug --target server-adapter`
Expected: 生成 `server-adapter` 静态库，无编译错误

- [ ] **Step 3.4: 格式检查**

Run: `clang-format --dry-run --Werror server/adapter/src/AdapterFactory.cpp`
Expected: 无输出，退出码 0

---

## Task 4: Business 类型与接口

**Files:**
- Create: `server/business/include/ServerTypes.h`
- Create: `server/business/include/IAuthService.h`
- Create: `server/business/include/IRoomManager.h`
- Create: `server/business/include/IMessageRouter.h`
- Create: `server/business/include/IClientNotifier.h`
- Create: `server/business/include/BusinessFactory.h`

- [ ] **Step 4.1: 写 `ServerTypes.h`**

```cpp
#pragma once

namespace Prism::Server::Business {

// 入站消息 type
inline constexpr const char* MSG_REGISTER = "register";
inline constexpr const char* MSG_LOGIN = "login";
inline constexpr const char* MSG_CREATE_ROOM = "create_room";
inline constexpr const char* MSG_JOIN_ROOM = "join_room";
inline constexpr const char* MSG_LEAVE_ROOM = "leave_room";
inline constexpr const char* MSG_PLAYBACK = "playback";
inline constexpr const char* MSG_CHAT = "chat";

// 出站消息 type
inline constexpr const char* MSG_ACK = "ack";
inline constexpr const char* MSG_ERROR = "error";
inline constexpr const char* MSG_ROOM_UPDATE = "room_update";

} // namespace Prism::Server::Business
```

- [ ] **Step 4.2: 写 `IAuthService.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace Prism::Server::Business {

/**
 * @class IAuthService
 * @brief 用户认证抽象接口
 */
class IAuthService {
public:
    virtual ~IAuthService() = default;

    /**
     * @brief 注册用户
     * @param username 用户名
     * @param password 明文密码
     * @return 成功返回 true，用户名已存在返回 false
     */
    virtual bool register_user(const std::string& username, const std::string& password) = 0;

    /**
     * @brief 登录
     * @param username 用户名
     * @param password 明文密码
     * @return 成功返回用户 ID，否则 nullopt
     */
    virtual std::optional<int64_t> login(const std::string& username, const std::string& password) = 0;
};

} // namespace Prism::Server::Business
```

- [ ] **Step 4.3: 写 `IRoomManager.h`**

```cpp
#pragma once

#include "DBAdapter.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Prism::Server::Business {

/**
 * @class IRoomManager
 * @brief 房间管理抽象接口
 */
class IRoomManager {
public:
    virtual ~IRoomManager() = default;

    /**
     * @brief 创建房间
     * @param room_id 房间 ID
     * @param name 房间名
     * @param owner_id 房主用户 ID
     * @return 成功返回 true，房间已存在返回 false
     */
    virtual bool create_room(const std::string& room_id,
                             const std::string& name,
                             int64_t owner_id) = 0;

    /**
     * @brief 加入房间
     * @param room_id 房间 ID
     * @param user_id 用户 ID
     * @return 成功返回 true
     */
    virtual bool join_room(const std::string& room_id, int64_t user_id) = 0;

    /**
     * @brief 离开房间
     * @param room_id 房间 ID
     * @param user_id 用户 ID
     * @return 成功返回 true
     */
    virtual bool leave_room(const std::string& room_id, int64_t user_id) = 0;

    /**
     * @brief 列出全部房间
     * @return 房间列表
     */
    virtual std::vector<Adapter::RoomRecord> list_rooms() = 0;
};

} // namespace Prism::Server::Business
```

- [ ] **Step 4.4: 写 `IMessageRouter.h`**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace Prism::Server::Business {

/**
 * @class IMessageRouter
 * @brief 消息路由抽象接口：解析入站消息并按类型转发
 */
class IMessageRouter {
public:
    virtual ~IMessageRouter() = default;

    /**
     * @brief 路由一条入站消息
     * @param sender_user_id 发送者用户 ID
     * @param json 原始 JSON 消息字符串
     */
    virtual void route(int64_t sender_user_id, const std::string& json) = 0;
};

} // namespace Prism::Server::Business
```

- [ ] **Step 4.5: 写 `IClientNotifier.h`**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace Prism::Server::Business {

/**
 * @class IClientNotifier
 * @brief 客户端消息推送抽象接口（由 Network 层实现）
 */
class IClientNotifier {
public:
    virtual ~IClientNotifier() = default;

    /**
     * @brief 向指定用户发送消息
     * @param user_id 用户 ID
     * @param message 消息内容（JSON 字符串）
     * @return 用户在线且发送成功返回 true
     */
    virtual bool send_to_user(int64_t user_id, const std::string& message) = 0;

    /**
     * @brief 向房间内所有在线成员广播消息
     * @param room_id 房间 ID
     * @param message 消息内容（JSON 字符串）
     * @return 成功返回 true
     */
    virtual bool broadcast_to_room(const std::string& room_id, const std::string& message) = 0;
};

} // namespace Prism::Server::Business
```

- [ ] **Step 4.6: 写 `BusinessFactory.h`**

```cpp
#pragma once

#include "CryptoAdapter.h"
#include "DBAdapter.h"
#include "IAuthService.h"
#include "IClientNotifier.h"
#include "IMessageRouter.h"
#include "IRoomManager.h"

#include <memory>

namespace Prism::Server::Business {

/**
 * @brief 创建认证服务实例
 * @param db 数据库适配器（不持有所有权）
 * @param crypto 密码学适配器（不持有所有权）
 */
std::unique_ptr<IAuthService> create_auth_service(Adapter::DBAdapter* db,
                                                  Adapter::CryptoAdapter* crypto);

/**
 * @brief 创建房间管理器实例
 * @param db 数据库适配器（不持有所有权）
 * @param notifier 客户端推送器（不持有所有权）
 */
std::unique_ptr<IRoomManager> create_room_manager(Adapter::DBAdapter* db,
                                                  IClientNotifier* notifier);

/**
 * @brief 创建消息路由器实例
 * @param db 数据库适配器（不持有所有权）
 * @param notifier 客户端推送器（不持有所有权）
 */
std::unique_ptr<IMessageRouter> create_message_router(Adapter::DBAdapter* db,
                                                      IClientNotifier* notifier);

} // namespace Prism::Server::Business
```

- [ ] **Step 4.7: 格式检查**

Run: `clang-format --dry-run --Werror server/business/include/*.h`
Expected: 无输出，退出码 0

---

## Task 5: Business 实现

**Files:**
- Create: `server/business/internal/AuthService.h`
- Create: `server/business/internal/RoomManager.h`
- Create: `server/business/internal/MessageRouter.h`
- Create: `server/business/src/AuthService.cpp`
- Create: `server/business/src/RoomManager.cpp`
- Create: `server/business/src/MessageRouter.cpp`

- [ ] **Step 5.1: 写 `AuthService.h`**

```cpp
#pragma once

#include "IAuthService.h"

namespace Prism::Server::Adapter {
class DBAdapter;
class CryptoAdapter;
} // namespace Prism::Server::Adapter

namespace Prism::Server::Business {

/**
 * @class AuthService
 * @brief 用户认证实现
 */
class AuthService : public IAuthService {
public:
    AuthService(Adapter::DBAdapter* db, Adapter::CryptoAdapter* crypto);
    ~AuthService() override = default;

    bool register_user(const std::string& username, const std::string& password) override;
    std::optional<int64_t> login(const std::string& username, const std::string& password) override;

private:
    Adapter::DBAdapter* db_;
    Adapter::CryptoAdapter* crypto_;
};

} // namespace Prism::Server::Business
```

- [ ] **Step 5.2: 写 `AuthService.cpp`**

```cpp
#include "AuthService.h"

#include "CryptoAdapter.h"
#include "DBAdapter.h"

#include <spdlog/spdlog.h>

namespace Prism::Server::Business {

AuthService::AuthService(Adapter::DBAdapter* db, Adapter::CryptoAdapter* crypto)
    : db_(db), crypto_(crypto) {}

bool AuthService::register_user(const std::string& username, const std::string& password) {
    if (db_->query_user_by_username(username).has_value()) {
        spdlog::warn("register_user: username '{}' already exists", username);
        return false;
    }

    Adapter::HashResult hash = crypto_->hash_password(password);
    if (hash.hash.empty()) {
        spdlog::error("register_user: hash_password failed for '{}'", username);
        return false;
    }

    Adapter::UserRecord user;
    user.username = username;
    user.password_hash = hash.hash;
    user.salt = hash.salt;

    bool ok = db_->insert_user(user);
    if (ok) {
        spdlog::info("register_user: '{}' registered", username);
    }
    return ok;
}

std::optional<int64_t> AuthService::login(const std::string& username, const std::string& password) {
    auto user = db_->query_user_by_username(username);
    if (!user.has_value()) {
        spdlog::warn("login: username '{}' not found", username);
        return std::nullopt;
    }

    if (!crypto_->verify_password(password, user->password_hash, user->salt)) {
        spdlog::warn("login: password mismatch for '{}'", username);
        return std::nullopt;
    }

    spdlog::info("login: '{}' logged in (id={})", username, user->id);
    return user->id;
}

} // namespace Prism::Server::Business
```

- [ ] **Step 5.3: 写 `RoomManager.h`**

```cpp
#pragma once

#include "IRoomManager.h"

namespace Prism::Server::Adapter {
class DBAdapter;
} // namespace Prism::Server::Adapter

namespace Prism::Server::Business {

class IClientNotifier;

/**
 * @class RoomManager
 * @brief 房间管理实现
 */
class RoomManager : public IRoomManager {
public:
    RoomManager(Adapter::DBAdapter* db, IClientNotifier* notifier);
    ~RoomManager() override = default;

    bool create_room(const std::string& room_id,
                     const std::string& name,
                     int64_t owner_id) override;
    bool join_room(const std::string& room_id, int64_t user_id) override;
    bool leave_room(const std::string& room_id, int64_t user_id) override;
    std::vector<Adapter::RoomRecord> list_rooms() override;

private:
    Adapter::DBAdapter* db_;
    IClientNotifier* notifier_;
};

} // namespace Prism::Server::Business
```

- [ ] **Step 5.4: 写 `RoomManager.cpp`**

```cpp
#include "RoomManager.h"

#include "DBAdapter.h"
#include "IClientNotifier.h"
#include "ServerTypes.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace Prism::Server::Business {

RoomManager::RoomManager(Adapter::DBAdapter* db, IClientNotifier* notifier)
    : db_(db), notifier_(notifier) {}

bool RoomManager::create_room(const std::string& room_id,
                              const std::string& name,
                              int64_t owner_id) {
    Adapter::RoomRecord room;
    room.room_id = room_id;
    room.name = name;
    room.owner_id = owner_id;

    if (!db_->insert_room(room)) {
        spdlog::warn("create_room: room '{}' already exists", room_id);
        return false;
    }
    spdlog::info("create_room: '{}' created by user {}", room_id, owner_id);
    return true;
}

bool RoomManager::join_room(const std::string& room_id, int64_t user_id) {
    if (!db_->query_room_by_id(room_id).has_value()) {
        spdlog::warn("join_room: room '{}' not found", room_id);
        return false;
    }

    Adapter::RoomMemberRecord member;
    member.room_id = room_id;
    member.user_id = user_id;
    if (!db_->insert_room_member(member)) {
        spdlog::warn("join_room: user {} already in room '{}'", user_id, room_id);
        return false;
    }
    spdlog::info("join_room: user {} joined '{}'", user_id, room_id);

    nlohmann::json message;
    message["type"] = MSG_ROOM_UPDATE;
    message["room_id"] = room_id;
    message["member_count"] = static_cast<int>(db_->query_room_members(room_id).size());
    notifier_->broadcast_to_room(room_id, message.dump());
    return true;
}

bool RoomManager::leave_room(const std::string& room_id, int64_t user_id) {
    if (!db_->delete_room_member(room_id, user_id)) {
        spdlog::warn("leave_room: user {} not in room '{}'", user_id, room_id);
        return false;
    }
    spdlog::info("leave_room: user {} left '{}'", user_id, room_id);
    return true;
}

std::vector<Adapter::RoomRecord> RoomManager::list_rooms() {
    return db_->query_rooms();
}

} // namespace Prism::Server::Business
```

- [ ] **Step 5.5: 写 `MessageRouter.h`**

```cpp
#pragma once

#include "IMessageRouter.h"

namespace Prism::Server::Adapter {
class DBAdapter;
} // namespace Prism::Server::Adapter

namespace Prism::Server::Business {

class IClientNotifier;

/**
 * @class MessageRouter
 * @brief 消息路由实现
 */
class MessageRouter : public IMessageRouter {
public:
    MessageRouter(Adapter::DBAdapter* db, IClientNotifier* notifier);
    ~MessageRouter() override = default;

    void route(int64_t sender_user_id, const std::string& json) override;

private:
    Adapter::DBAdapter* db_;
    IClientNotifier* notifier_;
};

} // namespace Prism::Server::Business
```

- [ ] **Step 5.6: 写 `MessageRouter.cpp`**

```cpp
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
```

- [ ] **Step 5.7: 格式检查**

Run: `clang-format --dry-run --Werror server/business/internal/*.h server/business/src/*.cpp`
Expected: 无输出，退出码 0

---

## Task 6: Business 工厂 + CMake

**Files:**
- Create: `server/business/src/BusinessFactory.cpp`
- Modify: `server/business/CMakeLists.txt`

- [ ] **Step 6.1: 写 `BusinessFactory.cpp`**

```cpp
#include "BusinessFactory.h"

#include "AuthService.h"
#include "MessageRouter.h"
#include "RoomManager.h"

namespace Prism::Server::Business {

std::unique_ptr<IAuthService> create_auth_service(Adapter::DBAdapter* db,
                                                  Adapter::CryptoAdapter* crypto) {
    return std::make_unique<AuthService>(db, crypto);
}

std::unique_ptr<IRoomManager> create_room_manager(Adapter::DBAdapter* db,
                                                  IClientNotifier* notifier) {
    return std::make_unique<RoomManager>(db, notifier);
}

std::unique_ptr<IMessageRouter> create_message_router(Adapter::DBAdapter* db,
                                                      IClientNotifier* notifier) {
    return std::make_unique<MessageRouter>(db, notifier);
}

} // namespace Prism::Server::Business
```

- [ ] **Step 6.2: 改 `server/business/CMakeLists.txt`**

把整份文件替换为：

```cmake
project(server-business)

message(STATUS "${PROJECT_NAME} is building")

find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)

if(SRC)
    message(STATUS "The ${PROJECT_NAME} of source file include:")
    foreach(file ${SRC})
        get_filename_component(name ${file} NAME)
        message(STATUS "${name}")
    endforeach()
    message(STATUS "")

    add_library(${PROJECT_NAME} STATIC ${SRC})
    target_include_directories(${PROJECT_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/internal
    )

    target_link_libraries(${PROJECT_NAME}
    PUBLIC
        $<$<TARGET_EXISTS:server-adapter>:server-adapter>

    PRIVATE
        nlohmann_json
        spdlog::spdlog_header_only
    )
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
```

- [ ] **Step 6.3: 编译 `server-business`**

Run: `cmake --build --preset windows-clang-x64-Debug --target server-business`
Expected: 生成 `server-business` 静态库，无编译错误

- [ ] **Step 6.4: 格式检查**

Run: `clang-format --dry-run --Werror server/business/src/BusinessFactory.cpp`
Expected: 无输出，退出码 0

---

## Task 7: Network 类型与接口

**Files:**
- Create: `server/network/include/ISignalingServer.h`
- Create: `server/network/include/IHttpApi.h`
- Create: `server/network/include/NetworkFactory.h`

- [ ] **Step 7.1: 写 `ISignalingServer.h`**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace Prism::Server::Business {
class IAuthService;
class IRoomManager;
class IMessageRouter;
class IClientNotifier;
} // namespace Prism::Server::Business

namespace Prism::Server::Network {

/**
 * @class ISignalingServer
 * @brief WebSocket 信令服务端抽象接口
 */
class ISignalingServer {
public:
    virtual ~ISignalingServer() = default;

    /**
     * @brief 暴露内部客户端推送器
     * @return IClientNotifier 指针（不转移所有权）
     */
    virtual Business::IClientNotifier* notifier() = 0;

    /**
     * @brief 注入业务接口，接收入站消息路由
     * @param auth 认证服务
     * @param rooms 房间管理
     * @param router 消息路由
     */
    virtual void set_handlers(Business::IAuthService* auth,
                              Business::IRoomManager* rooms,
                              Business::IMessageRouter* router) = 0;

    /**
     * @brief 启动服务（非阻塞，内部起线程）
     * @param host 监听地址
     * @param port 监听端口
     * @return 成功返回 true
     */
    virtual bool start(const std::string& host, uint16_t port) = 0;

    /**
     * @brief 停止服务
     */
    virtual void stop() = 0;

    /**
     * @brief 阻塞等待服务线程结束
     */
    virtual void wait() = 0;
};

} // namespace Prism::Server::Network
```

- [ ] **Step 7.2: 写 `IHttpApi.h`（仅接口声明）**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace Prism::Server::Network {

/**
 * @class IHttpApi
 * @brief HTTP API 抽象接口（接口先行，本阶段不实现）
 *
 * 预留：后续用 asio 实现 REST 接口。本阶段不建实现、不接线、不参与构建。
 */
class IHttpApi {
public:
    virtual ~IHttpApi() = default;

    /**
     * @brief 启动 HTTP 服务
     * @param host 监听地址
     * @param port 监听端口
     * @return 成功返回 true
     */
    virtual bool start(const std::string& host, uint16_t port) = 0;

    /**
     * @brief 停止 HTTP 服务
     */
    virtual void stop() = 0;
};

} // namespace Prism::Server::Network
```

- [ ] **Step 7.3: 写 `NetworkFactory.h`**

```cpp
#pragma once

#include "ISignalingServer.h"

#include <memory>

namespace Prism::Server::Network {

/**
 * @brief 创建 WebSocket 信令服务端实例
 * @return ISignalingServer 实例
 */
std::unique_ptr<ISignalingServer> create_signaling_server();

} // namespace Prism::Server::Network
```

- [ ] **Step 7.4: 格式检查**

Run: `clang-format --dry-run --Werror server/network/include/*.h`
Expected: 无输出，退出码 0

---

## Task 8: Network 实现

**Files:**
- Create: `server/network/internal/SessionManager.h`
- Create: `server/network/internal/IxWebSocketServer.h`
- Create: `server/network/src/SessionManager.cpp`
- Create: `server/network/src/IxWebSocketServer.cpp`

- [ ] **Step 8.1: 写 `SessionManager.h`**

```cpp
#pragma once

#include "IClientNotifier.h"

#include <ixwebsocket/IXWebSocket.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>

namespace Prism::Server::Network {

/**
 * @class SessionManager
 * @brief 会话管理：维护连接、用户、房间在线成员映射，实现 IClientNotifier
 */
class SessionManager : public Business::IClientNotifier {
public:
    SessionManager() = default;
    ~SessionManager() override = default;

    bool send_to_user(int64_t user_id, const std::string& message) override;
    bool broadcast_to_room(const std::string& room_id, const std::string& message) override;

    /**
     * @brief 注册一条连接
     * @param conn_id 连接 ID
     * @param socket WebSocket 指针（不持有所有权）
     */
    void register_connection(const std::string& conn_id, ix::WebSocket* socket);

    /**
     * @brief 注销一条连接（并清理其用户绑定与房间在线状态）
     * @param conn_id 连接 ID
     */
    void unregister_connection(const std::string& conn_id);

    /**
     * @brief 绑定用户到连接
     * @param user_id 用户 ID
     * @param conn_id 连接 ID
     */
    void bind_user(int64_t user_id, const std::string& conn_id);

    /**
     * @brief 查询连接对应的用户 ID
     * @param conn_id 连接 ID
     * @return 已绑定返回用户 ID，否则 nullopt
     */
    std::optional<int64_t> user_of(const std::string& conn_id);

    /**
     * @brief 记录用户加入房间（在线状态）
     * @param room_id 房间 ID
     * @param user_id 用户 ID
     */
    void add_room_member(const std::string& room_id, int64_t user_id);

    /**
     * @brief 记录用户离开房间（在线状态）
     * @param room_id 房间 ID
     * @param user_id 用户 ID
     */
    void remove_room_member(const std::string& room_id, int64_t user_id);

private:
    std::mutex mutex_;
    std::map<std::string, ix::WebSocket*> conn_id_to_socket_;
    std::map<int64_t, std::string> user_id_to_conn_id_;
    std::map<std::string, int64_t> conn_id_to_user_id_;
    std::map<std::string, std::set<int64_t>> room_id_to_members_;
};

} // namespace Prism::Server::Network
```

- [ ] **Step 8.2: 写 `SessionManager.cpp`**

```cpp
#include "SessionManager.h"

namespace Prism::Server::Network {

bool SessionManager::send_to_user(int64_t user_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto user_it = user_id_to_conn_id_.find(user_id);
    if (user_it == user_id_to_conn_id_.end()) {
        return false;
    }
    auto socket_it = conn_id_to_socket_.find(user_it->second);
    if (socket_it == conn_id_to_socket_.end()) {
        return false;
    }
    socket_it->second->send(message);
    return true;
}

bool SessionManager::broadcast_to_room(const std::string& room_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto room_it = room_id_to_members_.find(room_id);
    if (room_it == room_id_to_members_.end()) {
        return false;
    }
    for (int64_t user_id : room_it->second) {
        auto user_it = user_id_to_conn_id_.find(user_id);
        if (user_it == user_id_to_conn_id_.end()) {
            continue;
        }
        auto socket_it = conn_id_to_socket_.find(user_it->second);
        if (socket_it == conn_id_to_socket_.end()) {
            continue;
        }
        socket_it->second->send(message);
    }
    return true;
}

void SessionManager::register_connection(const std::string& conn_id, ix::WebSocket* socket) {
    std::lock_guard<std::mutex> lock(mutex_);
    conn_id_to_socket_[conn_id] = socket;
}

void SessionManager::unregister_connection(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    conn_id_to_socket_.erase(conn_id);

    auto user_it = conn_id_to_user_id_.find(conn_id);
    if (user_it == conn_id_to_user_id_.end()) {
        return;
    }
    int64_t user_id = user_it->second;
    conn_id_to_user_id_.erase(user_it);
    user_id_to_conn_id_.erase(user_id);

    for (auto& entry : room_id_to_members_) {
        entry.second.erase(user_id);
    }
}

void SessionManager::bind_user(int64_t user_id, const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    user_id_to_conn_id_[user_id] = conn_id;
    conn_id_to_user_id_[conn_id] = user_id;
}

std::optional<int64_t> SessionManager::user_of(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = conn_id_to_user_id_.find(conn_id);
    if (it == conn_id_to_user_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void SessionManager::add_room_member(const std::string& room_id, int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    room_id_to_members_[room_id].insert(user_id);
}

void SessionManager::remove_room_member(const std::string& room_id, int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = room_id_to_members_.find(room_id);
    if (it != room_id_to_members_.end()) {
        it->second.erase(user_id);
    }
}

} // namespace Prism::Server::Network
```

- [ ] **Step 8.3: 写 `IxWebSocketServer.h`**

```cpp
#pragma once

#include "ISignalingServer.h"

#include <ixwebsocket/IXWebSocketServer.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace Prism::Server::Network {

class SessionManager;

/**
 * @class IxWebSocketServer
 * @brief 基于 ixwebsocket 的 WebSocket 信令服务端实现
 */
class IxWebSocketServer : public ISignalingServer {
public:
    IxWebSocketServer();
    ~IxWebSocketServer() override;

    Business::IClientNotifier* notifier() override;
    void set_handlers(Business::IAuthService* auth,
                      Business::IRoomManager* rooms,
                      Business::IMessageRouter* router) override;
    bool start(const std::string& host, uint16_t port) override;
    void stop() override;
    void wait() override;

private:
    void handle_message(const std::shared_ptr<ix::ConnectionState>& conn,
                        ix::WebSocket& socket,
                        const ix::WebSocketMessagePtr& message);
    void handle_json_message(const std::string& conn_id,
                             ix::WebSocket& socket,
                             const std::string& payload);

    void handle_register(const std::string& conn_id, ix::WebSocket& socket, const nlohmann::json& message);
    void handle_login(const std::string& conn_id, ix::WebSocket& socket, const nlohmann::json& message);
    void handle_create_room(const std::string& conn_id, ix::WebSocket& socket, const nlohmann::json& message);
    void handle_join_room(const std::string& conn_id, ix::WebSocket& socket, const nlohmann::json& message);
    void handle_leave_room(const std::string& conn_id, ix::WebSocket& socket, const nlohmann::json& message);

    void send_ack(ix::WebSocket& socket, const std::string& for_type);
    void send_error(ix::WebSocket& socket, int code, const std::string& message);

    std::unique_ptr<SessionManager> session_manager_;

    Business::IAuthService* auth_{nullptr};
    Business::IRoomManager* rooms_{nullptr};
    Business::IMessageRouter* router_{nullptr};

    std::unique_ptr<ix::WebSocketServer> server_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

} // namespace Prism::Server::Network
```

- [ ] **Step 8.4: 写 `IxWebSocketServer.cpp`**

```cpp
#include "IxWebSocketServer.h"

#include "IAuthService.h"
#include "IMessageRouter.h"
#include "IRoomManager.h"
#include "ServerTypes.h"
#include "SessionManager.h"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketMessage.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace Prism::Server::Network {

IxWebSocketServer::IxWebSocketServer() {
    ix::initNetSystem();
    session_manager_ = std::make_unique<SessionManager>();
}

IxWebSocketServer::~IxWebSocketServer() {
    stop();
}

Business::IClientNotifier* IxWebSocketServer::notifier() {
    return session_manager_.get();
}

void IxWebSocketServer::set_handlers(Business::IAuthService* auth,
                                     Business::IRoomManager* rooms,
                                     Business::IMessageRouter* router) {
    auth_ = auth;
    rooms_ = rooms;
    router_ = router;
}

bool IxWebSocketServer::start(const std::string& host, uint16_t port) {
    server_ = std::make_unique<ix::WebSocketServer>(port, host);

    server_->setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> conn,
               ix::WebSocket& socket,
               const ix::WebSocketMessagePtr& message) {
            handle_message(conn, socket, message);
        });

    running_ = true;
    thread_ = std::thread([this]() {
        server_->listenAndStart();
        running_ = false;
    });

    spdlog::info("IxWebSocketServer: listening on {}:{}", host, port);
    return true;
}

void IxWebSocketServer::stop() {
    if (!running_) {
        return;
    }
    if (server_) {
        server_->stop();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    running_ = false;
}

void IxWebSocketServer::wait() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void IxWebSocketServer::handle_message(const std::shared_ptr<ix::ConnectionState>& conn,
                                       ix::WebSocket& socket,
                                       const ix::WebSocketMessagePtr& message) {
    std::string conn_id = conn->getId();

    switch (message->type) {
    case ix::WebSocketMessageType::Open:
        session_manager_->register_connection(conn_id, &socket);
        spdlog::info("connection {} opened", conn_id);
        break;
    case ix::WebSocketMessageType::Close:
        session_manager_->unregister_connection(conn_id);
        spdlog::info("connection {} closed", conn_id);
        break;
    case ix::WebSocketMessageType::Message:
        handle_json_message(conn_id, socket, message->str);
        break;
    default:
        break;
    }
}

void IxWebSocketServer::handle_json_message(const std::string& conn_id,
                                            ix::WebSocket& socket,
                                            const std::string& payload) {
    nlohmann::json message;
    try {
        message = nlohmann::json::parse(payload);
    } catch (const nlohmann::json::exception& e) {
        spdlog::warn("handle_json_message: invalid JSON: {}", e.what());
        send_error(socket, 400, "invalid json");
        return;
    }

    if (!message.contains("type") || !message["type"].is_string()) {
        send_error(socket, 400, "missing type");
        return;
    }

    std::string type = message["type"].get<std::string>();

    if (type == Business::MSG_REGISTER) {
        handle_register(conn_id, socket, message);
    } else if (type == Business::MSG_LOGIN) {
        handle_login(conn_id, socket, message);
    } else if (type == Business::MSG_CREATE_ROOM) {
        handle_create_room(conn_id, socket, message);
    } else if (type == Business::MSG_JOIN_ROOM) {
        handle_join_room(conn_id, socket, message);
    } else if (type == Business::MSG_LEAVE_ROOM) {
        handle_leave_room(conn_id, socket, message);
    } else {
        auto sender = session_manager_->user_of(conn_id);
        if (router_ != nullptr && sender.has_value()) {
            router_->route(*sender, payload);
        }
    }
}

void IxWebSocketServer::handle_register(const std::string& /*conn_id*/,
                                        ix::WebSocket& socket,
                                        const nlohmann::json& message) {
    if (auth_ == nullptr) {
        send_error(socket, 500, "auth not ready");
        return;
    }
    if (!message.contains("username") || !message.contains("password")) {
        send_error(socket, 400, "missing username or password");
        return;
    }
    std::string username = message["username"].get<std::string>();
    std::string password = message["password"].get<std::string>();
    if (auth_->register_user(username, password)) {
        send_ack(socket, Business::MSG_REGISTER);
    } else {
        send_error(socket, 409, "username already exists");
    }
}

void IxWebSocketServer::handle_login(const std::string& conn_id,
                                     ix::WebSocket& socket,
                                     const nlohmann::json& message) {
    if (auth_ == nullptr) {
        send_error(socket, 500, "auth not ready");
        return;
    }
    if (!message.contains("username") || !message.contains("password")) {
        send_error(socket, 400, "missing username or password");
        return;
    }
    std::string username = message["username"].get<std::string>();
    std::string password = message["password"].get<std::string>();
    auto user_id = auth_->login(username, password);
    if (user_id.has_value()) {
        session_manager_->bind_user(*user_id, conn_id);
        send_ack(socket, Business::MSG_LOGIN);
    } else {
        send_error(socket, 401, "invalid credentials");
    }
}

void IxWebSocketServer::handle_create_room(const std::string& conn_id,
                                           ix::WebSocket& socket,
                                           const nlohmann::json& message) {
    auto user_id = session_manager_->user_of(conn_id);
    if (!user_id.has_value()) {
        send_error(socket, 401, "not logged in");
        return;
    }
    if (!message.contains("room_id")) {
        send_error(socket, 400, "missing room_id");
        return;
    }
    std::string room_id = message["room_id"].get<std::string>();
    std::string name = message.value("name", room_id);
    if (rooms_ != nullptr && rooms_->create_room(room_id, name, *user_id)) {
        send_ack(socket, Business::MSG_CREATE_ROOM);
    } else {
        send_error(socket, 409, "room already exists");
    }
}

void IxWebSocketServer::handle_join_room(const std::string& conn_id,
                                         ix::WebSocket& socket,
                                         const nlohmann::json& message) {
    auto user_id = session_manager_->user_of(conn_id);
    if (!user_id.has_value()) {
        send_error(socket, 401, "not logged in");
        return;
    }
    if (!message.contains("room_id")) {
        send_error(socket, 400, "missing room_id");
        return;
    }
    std::string room_id = message["room_id"].get<std::string>();
    if (rooms_ != nullptr && rooms_->join_room(room_id, *user_id)) {
        session_manager_->add_room_member(room_id, *user_id);
        send_ack(socket, Business::MSG_JOIN_ROOM);
    } else {
        send_error(socket, 404, "room not found or already joined");
    }
}

void IxWebSocketServer::handle_leave_room(const std::string& conn_id,
                                          ix::WebSocket& socket,
                                          const nlohmann::json& message) {
    auto user_id = session_manager_->user_of(conn_id);
    if (!user_id.has_value()) {
        send_error(socket, 401, "not logged in");
        return;
    }
    if (!message.contains("room_id")) {
        send_error(socket, 400, "missing room_id");
        return;
    }
    std::string room_id = message["room_id"].get<std::string>();
    if (rooms_ != nullptr && rooms_->leave_room(room_id, *user_id)) {
        session_manager_->remove_room_member(room_id, *user_id);
        send_ack(socket, Business::MSG_LEAVE_ROOM);
    } else {
        send_error(socket, 404, "not in room");
    }
}

void IxWebSocketServer::send_ack(ix::WebSocket& socket, const std::string& for_type) {
    nlohmann::json ack;
    ack["type"] = Business::MSG_ACK;
    ack["ok"] = true;
    ack["for"] = for_type;
    socket.send(ack.dump());
}

void IxWebSocketServer::send_error(ix::WebSocket& socket, int code, const std::string& message) {
    nlohmann::json err;
    err["type"] = Business::MSG_ERROR;
    err["code"] = code;
    err["message"] = message;
    socket.send(err.dump());
}

} // namespace Prism::Server::Network
```

- [ ] **Step 8.5: 格式检查**

Run: `clang-format --dry-run --Werror server/network/internal/*.h server/network/src/*.cpp`
Expected: 无输出，退出码 0

---

## Task 9: Network 工厂 + CMake

**Files:**
- Create: `server/network/src/NetworkFactory.cpp`
- Modify: `server/network/CMakeLists.txt`

- [ ] **Step 9.1: 写 `NetworkFactory.cpp`**

```cpp
#include "NetworkFactory.h"

#include "IxWebSocketServer.h"

namespace Prism::Server::Network {

std::unique_ptr<ISignalingServer> create_signaling_server() {
    return std::make_unique<IxWebSocketServer>();
}

} // namespace Prism::Server::Network
```

- [ ] **Step 9.2: 改 `server/network/CMakeLists.txt`**

把整份文件替换为：

```cmake
project(server-network)

message(STATUS "${PROJECT_NAME} is building")

find_package(asio REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ixwebsocket REQUIRED)
find_package(spdlog REQUIRED)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)

if(SRC)
    message(STATUS "The ${PROJECT_NAME} of source file include:")
    foreach(file ${SRC})
        get_filename_component(name ${file} NAME)
        message(STATUS "${name}")
    endforeach()
    message(STATUS "")

    add_library(${PROJECT_NAME} STATIC ${SRC})
    target_include_directories(${PROJECT_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/internal
    )

    target_link_libraries(${PROJECT_NAME}
    PUBLIC
        $<$<TARGET_EXISTS:server-business>:server-business>

    PRIVATE
        asio::asio
        ixwebsocket::ixwebsocket
        nlohmann_json
        spdlog::spdlog_header_only
    )
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
```

- [ ] **Step 9.3: 编译 `server-network`**

Run: `cmake --build --preset windows-clang-x64-Debug --target server-network`
Expected: 生成 `server-network` 静态库，无编译错误

- [ ] **Step 9.4: 格式检查**

Run: `clang-format --dry-run --Werror server/network/src/NetworkFactory.cpp`
Expected: 无输出，退出码 0

---

## Task 10: main.cpp 组装根 + 全量构建 + 冒烟

**Files:**
- Modify: `server/main.cpp`（重写）
- Modify: `server/CMakeLists.txt`

- [ ] **Step 10.1: 重写 `server/main.cpp`**

把整份文件替换为：

```cpp
#include "AdapterFactory.h"
#include "BusinessFactory.h"
#include "NetworkFactory.h"

#include <spdlog/spdlog.h>

int main() {
    spdlog::set_level(spdlog::level::info);

    auto db = Prism::Server::Adapter::create_db_adapter();
    auto crypto = Prism::Server::Adapter::create_crypto_adapter();

    auto server = Prism::Server::Network::create_signaling_server();
    auto notifier = server->notifier();

    auto auth = Prism::Server::Business::create_auth_service(db.get(), crypto.get());
    auto rooms = Prism::Server::Business::create_room_manager(db.get(), notifier);
    auto router = Prism::Server::Business::create_message_router(db.get(), notifier);

    server->set_handlers(auth.get(), rooms.get(), router.get());
    server->start("0.0.0.0", 8080);

    spdlog::info("Prism-server running on 0.0.0.0:8080");
    server->wait();
    return 0;
}
```

- [ ] **Step 10.2: 改 `server/CMakeLists.txt`**

在 `project(Prism-server VERSION 0.0.1)` 之后、`add_subdirectory` 之前插入 `find_package(spdlog REQUIRED)`，并在 `target_link_libraries` 的 `PRIVATE` 列表末尾追加 `spdlog::spdlog_header_only`。

改后完整文件：

```cmake
project(Prism-server VERSION 0.0.1)

message(STATUS "")
message(STATUS "${PROJECT_NAME} is building, version is ${PROJECT_VERSION}")

find_package(spdlog REQUIRED)

add_subdirectory(adapter)
add_subdirectory(network)
add_subdirectory(business)

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        business/include
        adapter/include
        network/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        $<$<TARGET_EXISTS:server-adapter>:server-adapter>
        $<$<TARGET_EXISTS:server-network>:server-network>
        $<$<TARGET_EXISTS:server-business>:server-business>
        spdlog::spdlog_header_only
)
```

- [ ] **Step 10.3: 全量编译 `Prism-server`**

Run: `cmake --build --preset windows-clang-x64-Debug --target Prism-server`
Expected: 生成 `Prism-server` 可执行文件，无链接错误

- [ ] **Step 10.4: 格式检查（全部新文件）**

Run: `clang-format --dry-run --Werror server/main.cpp`
Expected: 无输出，退出码 0

- [ ] **Step 10.5: 冒烟测试（手动）**

启动：`./build/Windows/debug/server/Prism-server.exe`（或对应构建树里的产物路径）
另开终端，用 `websocat ws://127.0.0.1:8080`（或 ixwebsocket 自带客户端）依次发送并观察回包：

```json
{"type":"register","username":"alice","password":"secret"}
{"type":"login","username":"alice","password":"secret"}
{"type":"create_room","room_id":"r1","name":"Room One"}
{"type":"join_room","room_id":"r1"}
{"type":"playback","room_id":"r1","command":"play","value":0}
```

Expected: register→`ack(ok,for=register)`；login→`ack(ok,for=login)`；create_room→`ack`；join_room→`ack` + `room_update` 广播；playback→`playback` 广播（含 `sender`）。spdlog 输出对应 info 日志。

- [ ] **Step 10.6: clang-tidy 复核（可选，等价 pre-push）**

Run: `sh scripts/checks/tidy.sh build/Windows/debug`（或本机 `git push` 触发 pre-push）
Expected: 三条规则（`readability-identifier-naming`、`readability-function-size`、`cppcoreguidelines-virtual-class-destructor`）零命中

---

## Self-Review 记录

- **Spec 覆盖**：Adapter（Task 1–3）、Business（Task 4–6）、Network（Task 7–9）、main 组装（Task 10）、依赖倒置 IClientNotifier（Task 4.5 + 8）、JSON 协议（ServerTypes.h + IxWebSocketServer 路由）、CMake 变更（Task 3/6/9/10）。已全部落到任务。
- **偏差**：① 不建 `network/include/NetworkTypes.h`（无共享类型，避免空文件）；② `ISignalingServer` 增加 `wait()`（main 阻塞需要）。均已在上文标注。
- **延后项复核**：MySQL（#83）、单测（#54）、HTTP REST、IConfigAdapter/IFileStorageAdapter、TLS —— 均不在本计划，符合 spec §10。
- **类型一致性**：`RoomRecord`/`UserRecord`/`RoomMemberRecord` 在 `Prism::Server::Adapter`；`IRoomManager::list_rooms()` 返回 `std::vector<Adapter::RoomRecord>`，与 `DBAdapter::query_rooms()` 一致；`IClientNotifier` 由 `SessionManager` 实现，`notifier()` 返回 `SessionManager*`（向上转型为 `Business::IClientNotifier*`）。
