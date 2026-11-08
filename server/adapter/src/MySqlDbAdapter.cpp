#include "MySqlDbAdapter.h"

#include <spdlog/spdlog.h>

#include <string>
#include <vector>

namespace Prism::Server::Adapter {

namespace {

/**
 * @brief 转义字符串并加上单引号，供拼 SQL 使用
 * @param mysql MySQL 连接
 * @param value 原始字符串
 * @return 形如 'escaped' 的 SQL 字面量
 */
std::string escape_string(MYSQL* mysql, const std::string& value) {
    std::vector<char> buffer(value.size() * 2 + 1);
    unsigned long length = mysql_real_escape_string(mysql, buffer.data(), value.data(),
                                                    static_cast<unsigned long>(value.size()));
    return "'" + std::string(buffer.data(), length) + "'";
}

/**
 * @brief 执行一条 SQL，失败时记录错误日志
 * @param mysql MySQL 连接
 * @param sql SQL 语句
 * @return 成功返回 true
 */
bool exec_query(MYSQL* mysql, const std::string& sql) {
    if (mysql_query(mysql, sql.c_str()) != 0) {
        spdlog::error("MySqlDbAdapter: query failed: {}", mysql_error(mysql));
        return false;
    }
    return true;
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
        spdlog::error("MySqlDbAdapter: connect to {}:{} failed: {}", config_.host, config_.port,
                      mysql_error(mysql_));
        mysql_close(mysql_);
        mysql_ = nullptr;
        return false;
    }
    if (mysql_set_character_set(mysql_, "utf8mb4") != 0) {
        spdlog::warn("MySqlDbAdapter: set utf8mb4 failed: {}", mysql_error(mysql_));
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
    if (mysql_ == nullptr) {
        return false;
    }
    return mysql_ping(mysql_) == 0;
}

bool MySqlDbAdapter::insert_user(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    std::string sql = "INSERT INTO USERS (account, password_hash, salt) VALUES (";
    sql += escape_string(mysql_, user.username);
    sql += ", ";
    sql += escape_string(mysql_, user.password_hash);
    sql += ", ";
    sql += escape_string(mysql_, user.salt);
    sql += ")";
    return exec_query(mysql_, sql);
}

std::optional<UserRecord> MySqlDbAdapter::query_user_by_username(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return std::nullopt;
    }
    std::string sql = "SELECT id, account, password_hash, salt FROM USERS WHERE account = ";
    sql += escape_string(mysql_, username);
    if (!exec_query(mysql_, sql)) {
        return std::nullopt;
    }
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (result == nullptr) {
        return std::nullopt;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == nullptr) {
        mysql_free_result(result);
        return std::nullopt;
    }
    UserRecord record;
    record.id = row[0] != nullptr ? std::stoll(row[0]) : 0;
    record.username = row[1] != nullptr ? row[1] : "";
    record.password_hash = row[2] != nullptr ? row[2] : "";
    record.salt = row[3] != nullptr ? row[3] : "";
    mysql_free_result(result);
    return record;
}

std::optional<UserRecord> MySqlDbAdapter::query_user_by_id(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return std::nullopt;
    }
    std::string sql = "SELECT id, account, password_hash, salt FROM USERS WHERE id = ";
    sql += std::to_string(id);
    if (!exec_query(mysql_, sql)) {
        return std::nullopt;
    }
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (result == nullptr) {
        return std::nullopt;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == nullptr) {
        mysql_free_result(result);
        return std::nullopt;
    }
    UserRecord record;
    record.id = row[0] != nullptr ? std::stoll(row[0]) : 0;
    record.username = row[1] != nullptr ? row[1] : "";
    record.password_hash = row[2] != nullptr ? row[2] : "";
    record.salt = row[3] != nullptr ? row[3] : "";
    mysql_free_result(result);
    return record;
}

bool MySqlDbAdapter::insert_room(const RoomRecord& room) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    std::string sql = "INSERT INTO ROOMS (room_id, name, owner_id) VALUES (";
    sql += escape_string(mysql_, room.room_id);
    sql += ", ";
    sql += escape_string(mysql_, room.name);
    sql += ", ";
    sql += std::to_string(room.owner_id);
    sql += ")";
    return exec_query(mysql_, sql);
}

std::optional<RoomRecord> MySqlDbAdapter::query_room_by_id(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return std::nullopt;
    }
    std::string sql = "SELECT room_id, name, owner_id FROM ROOMS WHERE room_id = ";
    sql += escape_string(mysql_, room_id);
    if (!exec_query(mysql_, sql)) {
        return std::nullopt;
    }
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (result == nullptr) {
        return std::nullopt;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == nullptr) {
        mysql_free_result(result);
        return std::nullopt;
    }
    RoomRecord record;
    record.room_id = row[0] != nullptr ? row[0] : "";
    record.name = row[1] != nullptr ? row[1] : "";
    record.owner_id = row[2] != nullptr ? std::stoll(row[2]) : 0;
    mysql_free_result(result);
    return record;
}

std::vector<RoomRecord> MySqlDbAdapter::query_rooms() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RoomRecord> result;
    if (mysql_ == nullptr) {
        return result;
    }
    std::string sql = "SELECT room_id, name, owner_id FROM ROOMS";
    if (!exec_query(mysql_, sql)) {
        return result;
    }
    MYSQL_RES* res = mysql_store_result(mysql_);
    if (res == nullptr) {
        return result;
    }
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        RoomRecord record;
        record.room_id = row[0] != nullptr ? row[0] : "";
        record.name = row[1] != nullptr ? row[1] : "";
        record.owner_id = row[2] != nullptr ? std::stoll(row[2]) : 0;
        result.push_back(record);
    }
    mysql_free_result(res);
    return result;
}

bool MySqlDbAdapter::insert_room_member(const RoomMemberRecord& member) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    std::string sql = "INSERT INTO ROOM_MEMBERS (room_id, user_id) VALUES (";
    sql += escape_string(mysql_, member.room_id);
    sql += ", ";
    sql += std::to_string(member.user_id);
    sql += ")";
    return exec_query(mysql_, sql);
}

bool MySqlDbAdapter::delete_room_member(const std::string& room_id, int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mysql_ == nullptr) {
        return false;
    }
    std::string sql = "DELETE FROM ROOM_MEMBERS WHERE room_id = ";
    sql += escape_string(mysql_, room_id);
    sql += " AND user_id = ";
    sql += std::to_string(user_id);
    if (!exec_query(mysql_, sql)) {
        return false;
    }
    return mysql_affected_rows(mysql_) > 0;
}

std::vector<RoomMemberRecord> MySqlDbAdapter::query_room_members(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RoomMemberRecord> result;
    if (mysql_ == nullptr) {
        return result;
    }
    std::string sql = "SELECT room_id, user_id FROM ROOM_MEMBERS WHERE room_id = ";
    sql += escape_string(mysql_, room_id);
    if (!exec_query(mysql_, sql)) {
        return result;
    }
    MYSQL_RES* res = mysql_store_result(mysql_);
    if (res == nullptr) {
        return result;
    }
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        RoomMemberRecord member;
        member.room_id = row[0] != nullptr ? row[0] : "";
        member.user_id = row[1] != nullptr ? std::stoll(row[1]) : 0;
        result.push_back(member);
    }
    mysql_free_result(res);
    return result;
}

} // namespace Prism::Server::Adapter
