#include "MySqlDbAdapter.h"

#include <cstring>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace Prism::Server::Adapter {

namespace {

const char* room_state_to_str(RoomState state) {
    switch (state) {
    case RoomState::PLAYING:
        return "playing";
    case RoomState::PAUSED:
        return "paused";
    case RoomState::IDLE:
        return "idle";
    }
    return "idle";
}

RoomState room_state_from_str(const char* value) {
    if (value == nullptr) {
        return RoomState::IDLE;
    }
    if (std::strcmp(value, "playing") == 0) {
        return RoomState::PLAYING;
    }
    if (std::strcmp(value, "paused") == 0) {
        return RoomState::PAUSED;
    }
    return RoomState::IDLE;
}

// 将 std::string 绑定为输入参数（buffer 指向字符串内部，生命周期覆盖本方法）
void bind_string(MYSQL_BIND& bind, const std::string& value) {
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = const_cast<char*>(value.c_str());
    bind.buffer_length = static_cast<unsigned long>(value.size());
}

} // namespace

MySqlDbAdapter::MySqlDbAdapter(const MySqlConfig& config) : config_(config) {}

MySqlDbAdapter::~MySqlDbAdapter() {
    close();
}

bool MySqlDbAdapter::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ != nullptr) {
        return true;
    }
    mysql_ = mysql_init(nullptr);
    if (mysql_ == nullptr) {
        spdlog::error("MySqlDbAdapter: mysql_init failed");
        return false;
    }
    if (mysql_real_connect(mysql_, config_.host.c_str(), config_.user.c_str(),
                           config_.password.c_str(), config_.database.c_str(), config_.port,
                           nullptr, 0) == nullptr) {
        spdlog::error("MySqlDbAdapter: connect failed: {}", mysql_error(mysql_));
        mysql_close(mysql_);
        mysql_ = nullptr;
        return false;
    }
    spdlog::info("MySqlDbAdapter: connected to {}:{}/{}", config_.host, config_.port,
                 config_.database);
    return true;
}

void MySqlDbAdapter::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ != nullptr) {
        mysql_close(mysql_);
        mysql_ = nullptr;
        spdlog::info("MySqlDbAdapter: closed");
    }
}

bool MySqlDbAdapter::is_connected() {
    std::lock_guard<std::mutex> lock(mutex_);
    return mysql_ != nullptr;
}

bool MySqlDbAdapter::insert_user(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    const char* sql = "INSERT INTO users (username, password_hash, salt) VALUES (?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        spdlog::error("MySqlDbAdapter: insert_user prepare failed: {}",
                      stmt ? mysql_stmt_error(stmt) : mysql_error(mysql_));
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return false;
    }
    MYSQL_BIND bind[3] = {};
    bind_string(bind[0], user.username);
    bind_string(bind[1], user.password_hash);
    bind_string(bind[2], user.salt);
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        spdlog::error("MySqlDbAdapter: insert_user failed: {}", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    mysql_stmt_close(stmt);
    return true;
}

std::optional<UserRecord> MySqlDbAdapter::query_user_by_username(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return std::nullopt;
    }
    const char* sql = "SELECT id, username, password_hash, salt FROM users WHERE username = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return std::nullopt;
    }
    MYSQL_BIND bind[1] = {};
    bind_string(bind[0], username);
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    int64_t id = 0;
    char name_buf[65] = {};
    char hash_buf[129] = {};
    char salt_buf[65] = {};
    unsigned long name_len = 0;
    unsigned long hash_len = 0;
    unsigned long salt_len = 0;
    bool name_null = false;
    bool hash_null = false;
    bool salt_null = false;

    MYSQL_BIND result[4] = {};
    result[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result[0].buffer = &id;
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = name_buf;
    result[1].buffer_length = sizeof(name_buf) - 1;
    result[1].length = &name_len;
    result[1].is_null = &name_null;
    result[2].buffer_type = MYSQL_TYPE_STRING;
    result[2].buffer = hash_buf;
    result[2].buffer_length = sizeof(hash_buf) - 1;
    result[2].length = &hash_len;
    result[2].is_null = &hash_null;
    result[3].buffer_type = MYSQL_TYPE_STRING;
    result[3].buffer = salt_buf;
    result[3].buffer_length = sizeof(salt_buf) - 1;
    result[3].length = &salt_len;
    result[3].is_null = &salt_null;
    mysql_stmt_bind_result(stmt, result);

    int rc = mysql_stmt_fetch(stmt);
    mysql_stmt_close(stmt);
    if (rc != 0 && rc != MYSQL_DATA_TRUNCATED) {
        return std::nullopt;
    }
    UserRecord user;
    user.id = id;
    if (!name_null) {
        user.username.assign(name_buf, name_len);
    }
    if (!hash_null) {
        user.password_hash.assign(hash_buf, hash_len);
    }
    if (!salt_null) {
        user.salt.assign(salt_buf, salt_len);
    }
    return user;
}

std::optional<UserRecord> MySqlDbAdapter::query_user_by_id(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return std::nullopt;
    }
    const char* sql = "SELECT id, username, password_hash, salt FROM users WHERE id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return std::nullopt;
    }
    MYSQL_BIND bind[1] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[0].buffer = &id;
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    int64_t out_id = 0;
    char name_buf[65] = {};
    char hash_buf[129] = {};
    char salt_buf[65] = {};
    unsigned long name_len = 0;
    unsigned long hash_len = 0;
    unsigned long salt_len = 0;
    bool name_null = false;
    bool hash_null = false;
    bool salt_null = false;

    MYSQL_BIND result[4] = {};
    result[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result[0].buffer = &out_id;
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = name_buf;
    result[1].buffer_length = sizeof(name_buf) - 1;
    result[1].length = &name_len;
    result[1].is_null = &name_null;
    result[2].buffer_type = MYSQL_TYPE_STRING;
    result[2].buffer = hash_buf;
    result[2].buffer_length = sizeof(hash_buf) - 1;
    result[2].length = &hash_len;
    result[2].is_null = &hash_null;
    result[3].buffer_type = MYSQL_TYPE_STRING;
    result[3].buffer = salt_buf;
    result[3].buffer_length = sizeof(salt_buf) - 1;
    result[3].length = &salt_len;
    result[3].is_null = &salt_null;
    mysql_stmt_bind_result(stmt, result);

    int rc = mysql_stmt_fetch(stmt);
    mysql_stmt_close(stmt);
    if (rc != 0 && rc != MYSQL_DATA_TRUNCATED) {
        return std::nullopt;
    }
    UserRecord user;
    user.id = out_id;
    if (!name_null) {
        user.username.assign(name_buf, name_len);
    }
    if (!hash_null) {
        user.password_hash.assign(hash_buf, hash_len);
    }
    if (!salt_null) {
        user.salt.assign(salt_buf, salt_len);
    }
    return user;
}

bool MySqlDbAdapter::insert_room(const RoomRecord& room) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    const char* sql = "INSERT INTO rooms (room_id, name, owner_id, max_members, state, "
                      "current_media, position_ms) VALUES (?, ?, ?, ?, ?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return false;
    }
    std::string state = room_state_to_str(room.state);
    int64_t owner_id = room.owner_id;
    int64_t max_members = room.max_members;
    int64_t position_ms = room.position_ms;
    MYSQL_BIND bind[7] = {};
    bind_string(bind[0], room.room_id);
    bind_string(bind[1], room.name);
    bind[2].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[2].buffer = &owner_id;
    bind[3].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[3].buffer = &max_members;
    bind_string(bind[4], state);
    bind_string(bind[5], room.current_media);
    bind[6].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[6].buffer = &position_ms;
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        spdlog::error("MySqlDbAdapter: insert_room failed: {}", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    mysql_stmt_close(stmt);
    return true;
}

std::optional<RoomRecord> MySqlDbAdapter::query_room_by_id(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return std::nullopt;
    }
    const char* sql = "SELECT room_id, name, owner_id, max_members, state, current_media, "
                      "position_ms FROM rooms WHERE room_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return std::nullopt;
    }
    MYSQL_BIND bind[1] = {};
    bind_string(bind[0], room_id);
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    char id_buf[65] = {};
    char name_buf[129] = {};
    char state_buf[17] = {};
    char media_buf[513] = {};
    int64_t owner_id = 0;
    int64_t max_members = 0;
    int64_t position_ms = 0;
    unsigned long id_len = 0;
    unsigned long name_len = 0;
    unsigned long state_len = 0;
    unsigned long media_len = 0;
    bool id_null = false;
    bool name_null = false;
    bool state_null = false;
    bool media_null = false;

    MYSQL_BIND result[7] = {};
    result[0].buffer_type = MYSQL_TYPE_STRING;
    result[0].buffer = id_buf;
    result[0].buffer_length = sizeof(id_buf) - 1;
    result[0].length = &id_len;
    result[0].is_null = &id_null;
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = name_buf;
    result[1].buffer_length = sizeof(name_buf) - 1;
    result[1].length = &name_len;
    result[1].is_null = &name_null;
    result[2].buffer_type = MYSQL_TYPE_LONGLONG;
    result[2].buffer = &owner_id;
    result[3].buffer_type = MYSQL_TYPE_LONGLONG;
    result[3].buffer = &max_members;
    result[4].buffer_type = MYSQL_TYPE_STRING;
    result[4].buffer = state_buf;
    result[4].buffer_length = sizeof(state_buf) - 1;
    result[4].length = &state_len;
    result[4].is_null = &state_null;
    result[5].buffer_type = MYSQL_TYPE_STRING;
    result[5].buffer = media_buf;
    result[5].buffer_length = sizeof(media_buf) - 1;
    result[5].length = &media_len;
    result[5].is_null = &media_null;
    result[6].buffer_type = MYSQL_TYPE_LONGLONG;
    result[6].buffer = &position_ms;
    mysql_stmt_bind_result(stmt, result);

    int rc = mysql_stmt_fetch(stmt);
    mysql_stmt_close(stmt);
    if (rc != 0 && rc != MYSQL_DATA_TRUNCATED) {
        return std::nullopt;
    }
    RoomRecord room;
    if (!id_null) {
        room.room_id.assign(id_buf, id_len);
    }
    if (!name_null) {
        room.name.assign(name_buf, name_len);
    }
    room.owner_id = owner_id;
    room.max_members = static_cast<int32_t>(max_members);
    room.state = state_null ? RoomState::IDLE : room_state_from_str(state_buf);
    if (!media_null) {
        room.current_media.assign(media_buf, media_len);
    }
    room.position_ms = position_ms;
    return room;
}

std::vector<RoomRecord> MySqlDbAdapter::query_rooms() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RoomRecord> rooms;
    if (mysql_ == nullptr) {
        return rooms;
    }
    const char* sql = "SELECT room_id, name, owner_id, max_members, state, current_media, "
                      "position_ms FROM rooms";
    if (mysql_query(mysql_, sql) != 0) {
        spdlog::error("MySqlDbAdapter: query_rooms failed: {}", mysql_error(mysql_));
        return rooms;
    }
    MYSQL_RES* res = mysql_store_result(mysql_);
    if (res == nullptr) {
        return rooms;
    }
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        RoomRecord room;
        room.room_id = row[0] ? row[0] : "";
        room.name = row[1] ? row[1] : "";
        room.owner_id = row[2] ? std::stoll(row[2]) : 0;
        room.max_members = row[3] ? std::stoi(row[3]) : 0;
        room.state = room_state_from_str(row[4]);
        room.current_media = row[5] ? row[5] : "";
        room.position_ms = row[6] ? std::stoll(row[6]) : 0;
        rooms.push_back(std::move(room));
    }
    mysql_free_result(res);
    return rooms;
}

bool MySqlDbAdapter::insert_room_member(const RoomMemberRecord& member) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    const char* sql = "INSERT INTO room_members (room_id, user_id) VALUES (?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return false;
    }
    int64_t user_id = member.user_id;
    MYSQL_BIND bind[2] = {};
    bind_string(bind[0], member.room_id);
    bind[1].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[1].buffer = &user_id;
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        spdlog::error("MySqlDbAdapter: insert_room_member failed: {}", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    mysql_stmt_close(stmt);
    return true;
}

bool MySqlDbAdapter::delete_room_member(const std::string& room_id, int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    const char* sql = "DELETE FROM room_members WHERE room_id = ? AND user_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return false;
    }
    MYSQL_BIND bind[2] = {};
    bind_string(bind[0], room_id);
    bind[1].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[1].buffer = &user_id;
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        spdlog::error("MySqlDbAdapter: delete_room_member failed: {}", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    bool affected = mysql_stmt_affected_rows(stmt) > 0;
    mysql_stmt_close(stmt);
    return affected;
}

std::vector<RoomMemberRecord> MySqlDbAdapter::query_room_members(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RoomMemberRecord> members;
    if (mysql_ == nullptr) {
        return members;
    }
    const char* sql = "SELECT room_id, user_id, UNIX_TIMESTAMP(joined_at) * 1000 FROM room_members "
                      "WHERE room_id = ? ORDER BY joined_at ASC";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return members;
    }
    MYSQL_BIND bind[1] = {};
    bind_string(bind[0], room_id);
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return members;
    }

    char id_buf[65] = {};
    int64_t user_id = 0;
    int64_t joined_ms = 0;
    unsigned long id_len = 0;
    bool id_null = false;

    MYSQL_BIND result[3] = {};
    result[0].buffer_type = MYSQL_TYPE_STRING;
    result[0].buffer = id_buf;
    result[0].buffer_length = sizeof(id_buf) - 1;
    result[0].length = &id_len;
    result[0].is_null = &id_null;
    result[1].buffer_type = MYSQL_TYPE_LONGLONG;
    result[1].buffer = &user_id;
    result[2].buffer_type = MYSQL_TYPE_LONGLONG;
    result[2].buffer = &joined_ms;
    mysql_stmt_bind_result(stmt, result);

    int rc = 0;
    while ((rc = mysql_stmt_fetch(stmt)) == 0 || rc == MYSQL_DATA_TRUNCATED) {
        RoomMemberRecord member;
        if (!id_null) {
            member.room_id.assign(id_buf, id_len);
        }
        member.user_id = user_id;
        member.joined_at = joined_ms;
        members.push_back(member);
    }
    mysql_stmt_close(stmt);
    return members;
}

bool MySqlDbAdapter::update_room_state(const std::string& room_id, RoomState state,
                                       const std::string& current_media, int64_t position_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    const char* sql = "UPDATE rooms SET state = ?, current_media = ?, position_ms = ? "
                      "WHERE room_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return false;
    }
    std::string state_str = room_state_to_str(state);
    MYSQL_BIND bind[4] = {};
    bind_string(bind[0], state_str);
    bind_string(bind[1], current_media);
    bind[2].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[2].buffer = &position_ms;
    bind_string(bind[3], room_id);
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        spdlog::error("MySqlDbAdapter: update_room_state failed: {}", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    bool affected = mysql_stmt_affected_rows(stmt) > 0;
    mysql_stmt_close(stmt);
    return affected;
}

bool MySqlDbAdapter::update_room_owner(const std::string& room_id, int64_t owner_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    const char* sql = "UPDATE rooms SET owner_id = ? WHERE room_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return false;
    }
    MYSQL_BIND bind[2] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[0].buffer = &owner_id;
    bind_string(bind[1], room_id);
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        spdlog::error("MySqlDbAdapter: update_room_owner failed: {}", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    bool affected = mysql_stmt_affected_rows(stmt) > 0;
    mysql_stmt_close(stmt);
    return affected;
}

bool MySqlDbAdapter::delete_room(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    const char* sql = "DELETE FROM rooms WHERE room_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (stmt == nullptr ||
        mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
        return false;
    }
    MYSQL_BIND bind[1] = {};
    bind_string(bind[0], room_id);
    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        spdlog::error("MySqlDbAdapter: delete_room failed: {}", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    mysql_stmt_close(stmt);
    return true;
}

} // namespace Prism::Server::Adapter
