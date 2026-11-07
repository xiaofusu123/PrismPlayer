#include "SQLiteDBAdapter.h"

#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

namespace Prism::Adapter {
namespace {

// ==================== DDL ====================

constexpr const char* CREATE_ACCOUNTS_SQL = R"SQL(
CREATE TABLE IF NOT EXISTS accounts (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT    NOT NULL UNIQUE,
    email         TEXT    NOT NULL DEFAULT '',
    password_hash TEXT    NOT NULL DEFAULT ''
))SQL";

constexpr const char* CREATE_AUDIO_SQL = R"SQL(
CREATE TABLE IF NOT EXISTS audio_files (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    title           TEXT    NOT NULL DEFAULT '',
    file_path       TEXT    NOT NULL DEFAULT '',
    file_size       INTEGER NOT NULL DEFAULT 0,
    cover_path      TEXT    NOT NULL DEFAULT '',
    artist          TEXT    NOT NULL DEFAULT '',
    encoding_format TEXT    NOT NULL DEFAULT '',
    duration        INTEGER NOT NULL DEFAULT 0,
    bit_depth       INTEGER NOT NULL DEFAULT 0,
    sample_rate     INTEGER NOT NULL DEFAULT 0,
    bit_rate        INTEGER NOT NULL DEFAULT 0,
    channels        INTEGER NOT NULL DEFAULT 0,
    channel_layout  TEXT    NOT NULL DEFAULT '',
    lyrics_path     TEXT    NOT NULL DEFAULT ''
))SQL";

constexpr const char* CREATE_VIDEO_SQL = R"SQL(
CREATE TABLE IF NOT EXISTS video_files (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    filename          TEXT    NOT NULL DEFAULT '',
    file_path         TEXT    NOT NULL DEFAULT '',
    file_size         INTEGER NOT NULL DEFAULT 0,
    author            TEXT    NOT NULL DEFAULT '',
    resolution_width  INTEGER NOT NULL DEFAULT 0,
    resolution_height INTEGER NOT NULL DEFAULT 0,
    color_depth       INTEGER NOT NULL DEFAULT 0,
    duration          INTEGER NOT NULL DEFAULT 0,
    encoding_format   TEXT    NOT NULL DEFAULT '',
    frame_rate        REAL    NOT NULL DEFAULT 0.0,
    bit_rate          INTEGER NOT NULL DEFAULT 0
))SQL";

constexpr const char* CREATE_SETTINGS_SQL = R"SQL(
CREATE TABLE IF NOT EXISTS settings (
    "key"   TEXT PRIMARY KEY NOT NULL,
    "value" TEXT NOT NULL DEFAULT ''
))SQL";

// ==================== Account ====================

constexpr const char* INSERT_ACCOUNT_SQL =
    "INSERT INTO accounts(username, email, password_hash) VALUES(?, ?, ?)";

constexpr const char* SELECT_ACCOUNT_SQL =
    "SELECT id, username, email, password_hash FROM accounts WHERE username = ? LIMIT 1";

constexpr const char* UPDATE_ACCOUNT_SQL =
    "UPDATE accounts SET username = ?, email = ?, password_hash = ? WHERE id = ?";

constexpr const char* DELETE_ACCOUNT_SQL = "DELETE FROM accounts WHERE id = ?";

// ==================== Audio ====================

constexpr const char* INSERT_AUDIO_SQL =
    "INSERT INTO audio_files(title, file_path, file_size, cover_path, artist, encoding_format, "
    "duration, bit_depth, sample_rate, bit_rate, channels, channel_layout, lyrics_path) "
    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

constexpr const char* SELECT_AUDIO_ALL_SQL =
    "SELECT id, title, file_path, file_size, cover_path, artist, encoding_format, duration, "
    "bit_depth, sample_rate, bit_rate, channels, channel_layout, lyrics_path "
    "FROM audio_files ORDER BY id";

constexpr const char* SELECT_AUDIO_BY_ID_SQL =
    "SELECT id, title, file_path, file_size, cover_path, artist, encoding_format, duration, "
    "bit_depth, sample_rate, bit_rate, channels, channel_layout, lyrics_path "
    "FROM audio_files WHERE id = ? LIMIT 1";

constexpr const char* UPDATE_AUDIO_SQL =
    "UPDATE audio_files SET title = ?, file_path = ?, file_size = ?, cover_path = ?, artist = ?, "
    "encoding_format = ?, duration = ?, bit_depth = ?, sample_rate = ?, bit_rate = ?, channels = "
    "?, "
    "channel_layout = ?, lyrics_path = ? WHERE id = ?";

constexpr const char* DELETE_AUDIO_SQL = "DELETE FROM audio_files WHERE id = ?";

// ==================== Video ====================

constexpr const char* INSERT_VIDEO_SQL =
    "INSERT INTO video_files(filename, file_path, file_size, author, resolution_width, "
    "resolution_height, color_depth, duration, encoding_format, frame_rate, bit_rate) "
    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

constexpr const char* SELECT_VIDEO_ALL_SQL =
    "SELECT id, filename, file_path, file_size, author, resolution_width, resolution_height, "
    "color_depth, duration, encoding_format, frame_rate, bit_rate "
    "FROM video_files ORDER BY id";

constexpr const char* SELECT_VIDEO_BY_ID_SQL =
    "SELECT id, filename, file_path, file_size, author, resolution_width, resolution_height, "
    "color_depth, duration, encoding_format, frame_rate, bit_rate "
    "FROM video_files WHERE id = ? LIMIT 1";

constexpr const char* UPDATE_VIDEO_SQL =
    "UPDATE video_files SET filename = ?, file_path = ?, file_size = ?, author = ?, "
    "resolution_width = ?, resolution_height = ?, color_depth = ?, duration = ?, "
    "encoding_format = ?, frame_rate = ?, bit_rate = ? WHERE id = ?";

constexpr const char* DELETE_VIDEO_SQL = "DELETE FROM video_files WHERE id = ?";

// ==================== Settings ====================

constexpr const char* SELECT_SETTING_SQL =
    "SELECT \"value\" FROM settings WHERE \"key\" = ? LIMIT 1";

constexpr const char* UPSERT_SETTING_SQL =
    "INSERT INTO settings(\"key\", \"value\") VALUES(?, ?) "
    "ON CONFLICT(\"key\") DO UPDATE SET \"value\" = excluded.\"value\"";

// ==================== 辅助类型 ====================

/**
 * @class Statement
 * @brief sqlite3_stmt 的 RAII 包装，独占语句所有权
 *
 * 不做拷贝（语句句柄是独占资源），析构时 finalize。
 */
class Statement {
  public:
    Statement() = default;
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    /**
     * @brief 编译 SQL 并把语句绑定到连接
     * @param db 数据库连接
     * @param sql SQL 文本，只允许编译期常量
     * @return 编译成功返回 true
     */
    bool prepare(sqlite3* db, const char* sql);

    bool bind_int(int index, int64_t value);
    bool bind_uint(int index, uint64_t value);
    bool bind_text(int index, const std::string& value);
    bool bind_double(int index, double value);

    /**
     * @brief 推进一步
     * @return SQLITE_ROW / SQLITE_DONE / 错误码
     */
    [[nodiscard]] int step() const;

    [[nodiscard]] int64_t column_int(int index) const;
    [[nodiscard]] uint64_t column_uint(int index) const;
    [[nodiscard]] double column_double(int index) const;
    [[nodiscard]] std::string column_text(int index) const;

  private:
    sqlite3* db_{nullptr};        /**< 仅用于取 sqlite3_errmsg，不持有 */
    sqlite3_stmt* stmt_{nullptr}; /**< 语句句柄，析构时 finalize */
};

// ==================== 辅助函数 ====================

/**
 * @brief uint64 写入 INTEGER 前的钳制
 * @param value 待写入的无符号值
 * @return 不超过 INT64_MAX 的有符号值
 *
 * 真实媒体文件的 size / duration / bitrate 不可能接近 2^63；
 * 这里钳制而不是报错，避免一个脏字段让整条插入失败。
 */
int64_t to_sql_int(uint64_t value) {
    auto max_value = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    if (value > max_value) {
        return std::numeric_limits<int64_t>::max();
    }
    return static_cast<int64_t>(value);
}

/**
 * @brief 把读回的 INTEGER 转回 uint64
 * @param value 读回的有符号值
 * @return 负数一律归 0
 */
uint64_t to_uint(int64_t value) {
    if (value < 0) {
        return 0;
    }
    return static_cast<uint64_t>(value);
}

/**
 * @brief 执行一条不求结果的 SQL（DDL / 事务控制）
 * @param db 数据库连接
 * @param sql SQL 文本
 * @return 成功返回 true
 */
bool exec_sql(sqlite3* db, const char* sql) {
    char* error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        spdlog::error("[DBAdapter] exec failed: {} (sql: {})",
                      error != nullptr ? error : "unknown error", sql);
        sqlite3_free(error);
        return false;
    }
    return true;
}

/**
 * @brief 在事务里建齐 4 张表
 * @param db 数据库连接
 * @return 全部成功返回 true；任一失败会回滚
 */
bool create_schema(sqlite3* db) {
    if (!exec_sql(db, "BEGIN IMMEDIATE")) {
        return false;
    }

    bool ok = exec_sql(db, CREATE_ACCOUNTS_SQL) && exec_sql(db, CREATE_AUDIO_SQL) &&
              exec_sql(db, CREATE_VIDEO_SQL) && exec_sql(db, CREATE_SETTINGS_SQL);
    if (!ok) {
        exec_sql(db, "ROLLBACK");
        return false;
    }

    if (!exec_sql(db, "COMMIT")) {
        exec_sql(db, "ROLLBACK");
        return false;
    }
    return true;
}

/**
 * @brief 把当前行读进 AccountInfo
 * @param stmt 已定位到一行的语句
 * @param info 输出结构体
 */
void read_account_row(const Statement& stmt, AccountInfo& info) {
    info.id = stmt.column_int(0);
    info.username = stmt.column_text(1);
    info.email = stmt.column_text(2);
    info.password_hash = stmt.column_text(3);
}

/**
 * @brief 把当前行读进 AudioFileInfo
 * @param stmt 已定位到一行的语句
 * @param info 输出结构体
 */
void read_audio_row(const Statement& stmt, AudioFileInfo& info) {
    info.id = stmt.column_int(0);
    info.title = stmt.column_text(1);
    info.file_path = stmt.column_text(2);
    info.file_size = stmt.column_uint(3);
    info.cover_path = stmt.column_text(4);
    info.artist = stmt.column_text(5);
    info.encoding_format = stmt.column_text(6);
    info.duration = stmt.column_uint(7);
    info.bit_depth = static_cast<uint32_t>(stmt.column_uint(8));
    info.sample_rate = static_cast<uint32_t>(stmt.column_uint(9));
    info.bit_rate = stmt.column_uint(10);
    info.channels = static_cast<uint32_t>(stmt.column_uint(11));
    info.channel_layout = stmt.column_text(12);
    info.lyrics_path = stmt.column_text(13);
}

/**
 * @brief 把当前行读进 VideoFileInfo
 * @param stmt 已定位到一行的语句
 * @param info 输出结构体
 */
void read_video_row(const Statement& stmt, VideoFileInfo& info) {
    info.id = stmt.column_int(0);
    info.filename = stmt.column_text(1);
    info.file_path = stmt.column_text(2);
    info.file_size = stmt.column_uint(3);
    info.author = stmt.column_text(4);
    info.resolution_width = static_cast<uint32_t>(stmt.column_uint(5));
    info.resolution_height = static_cast<uint32_t>(stmt.column_uint(6));
    info.color_depth = static_cast<uint32_t>(stmt.column_uint(7));
    info.duration = stmt.column_uint(8);
    info.encoding_format = stmt.column_text(9);
    info.frame_rate = stmt.column_double(10);
    info.bit_rate = stmt.column_uint(11);
}

// ==================== Statement 实现 ====================

Statement::~Statement() {
    if (stmt_ != nullptr) {
        sqlite3_finalize(stmt_);
    }
}

bool Statement::prepare(sqlite3* db, const char* sql) {
    if (stmt_ != nullptr) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }

    db_ = db;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
        spdlog::error("[DBAdapter] prepare failed: {} (sql: {})", sqlite3_errmsg(db), sql);
        stmt_ = nullptr;
        return false;
    }
    return true;
}

bool Statement::bind_int(int index, int64_t value) {
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
        spdlog::error("[DBAdapter] bind_int({}) failed: {}", index, sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

bool Statement::bind_uint(int index, uint64_t value) {
    return bind_int(index, to_sql_int(value));
}

bool Statement::bind_text(int index, const std::string& value) {
    // SQLITE_TRANSIENT：让 SQLite 立刻复制一份。用 SQLITE_STATIC 时若 value 是临时对象，
    // step 之前缓冲区就可能失效。
    if (sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
        spdlog::error("[DBAdapter] bind_text({}) failed: {}", index, sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

bool Statement::bind_double(int index, double value) {
    if (sqlite3_bind_double(stmt_, index, value) != SQLITE_OK) {
        spdlog::error("[DBAdapter] bind_double({}) failed: {}", index, sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

int Statement::step() const {
    return sqlite3_step(stmt_);
}

int64_t Statement::column_int(int index) const {
    return sqlite3_column_int64(stmt_, index);
}

uint64_t Statement::column_uint(int index) const {
    return to_uint(sqlite3_column_int64(stmt_, index));
}

double Statement::column_double(int index) const {
    return sqlite3_column_double(stmt_, index);
}

std::string Statement::column_text(int index) const {
    auto* text = sqlite3_column_text(stmt_, index);
    if (text == nullptr) {
        return {};
    }
    auto bytes = sqlite3_column_bytes(stmt_, index);
    return std::string(reinterpret_cast<const char*>(text), static_cast<size_t>(bytes));
}

} // namespace

// ==================== 生命周期 ====================

SQLiteDBAdapter::~SQLiteDBAdapter() {
    close();
}

bool SQLiteDBAdapter::init(const std::string& db_path) {
    close();

    if (db_path.empty()) {
        spdlog::error("[DBAdapter] init() called with an empty path");
        return false;
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(db_path.c_str(), &db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("[DBAdapter] cannot open '{}': {}", db_path,
                      db != nullptr ? sqlite3_errmsg(db) : "unknown error");
        if (db != nullptr) {
            sqlite3_close_v2(db);
        }
        return false;
    }

    if (!create_schema(db)) {
        spdlog::error("[DBAdapter] cannot create schema in '{}', closing connection", db_path);
        sqlite3_close_v2(db);
        return false;
    }

    db_ = db;
    spdlog::debug("[DBAdapter] opened '{}' (accounts / audio_files / video_files / settings)",
                  db_path);
    return true;
}

void SQLiteDBAdapter::close() {
    if (db_ == nullptr) {
        return;
    }

    sqlite3_close_v2(db_);
    db_ = nullptr;
    spdlog::debug("[DBAdapter] closed");
}

// ==================== Account ====================

bool SQLiteDBAdapter::insert_account(const AccountInfo& info) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] insert_account() called before init()");
        return false;
    }

    Statement stmt;
    if (!stmt.prepare(db_, INSERT_ACCOUNT_SQL)) {
        return false;
    }
    if (!stmt.bind_text(1, info.username) || !stmt.bind_text(2, info.email) ||
        !stmt.bind_text(3, info.password_hash)) {
        return false;
    }
    if (stmt.step() != SQLITE_DONE) {
        spdlog::error("[DBAdapter] insert_account() failed: {}", sqlite3_errmsg(db_));
        return false;
    }

    spdlog::debug("[DBAdapter] insert_account('{}') ok, rowid = {} (not returned to caller)",
                  info.username, sqlite3_last_insert_rowid(db_));
    return true;
}

std::optional<AccountInfo> SQLiteDBAdapter::query_account_by_username(const std::string& username) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] query_account_by_username() called before init()");
        return std::nullopt;
    }

    Statement stmt;
    if (!stmt.prepare(db_, SELECT_ACCOUNT_SQL) || !stmt.bind_text(1, username)) {
        return std::nullopt;
    }

    int rc = stmt.step();
    if (rc == SQLITE_DONE) {
        spdlog::debug("[DBAdapter] query_account_by_username('{}'): no row", username);
        return std::nullopt;
    }
    if (rc != SQLITE_ROW) {
        spdlog::error("[DBAdapter] query_account_by_username() failed: {}", sqlite3_errmsg(db_));
        return std::nullopt;
    }

    AccountInfo info;
    read_account_row(stmt, info);
    return info;
}

bool SQLiteDBAdapter::update_account(const AccountInfo& info) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] update_account() called before init()");
        return false;
    }

    Statement stmt;
    if (!stmt.prepare(db_, UPDATE_ACCOUNT_SQL)) {
        return false;
    }
    if (!stmt.bind_text(1, info.username) || !stmt.bind_text(2, info.email) ||
        !stmt.bind_text(3, info.password_hash) || !stmt.bind_int(4, info.id)) {
        return false;
    }
    if (stmt.step() != SQLITE_DONE) {
        spdlog::error("[DBAdapter] update_account() failed: {}", sqlite3_errmsg(db_));
        return false;
    }
    if (sqlite3_changes(db_) == 0) {
        spdlog::warn("[DBAdapter] update_account(): no row with id={}", info.id);
        return false;
    }
    return true;
}

bool SQLiteDBAdapter::delete_account(int64_t id) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] delete_account() called before init()");
        return false;
    }

    Statement stmt;
    if (!stmt.prepare(db_, DELETE_ACCOUNT_SQL) || !stmt.bind_int(1, id)) {
        return false;
    }
    if (stmt.step() != SQLITE_DONE) {
        spdlog::error("[DBAdapter] delete_account() failed: {}", sqlite3_errmsg(db_));
        return false;
    }
    if (sqlite3_changes(db_) == 0) {
        spdlog::warn("[DBAdapter] delete_account(): no row with id={}", id);
        return false;
    }
    return true;
}

// ==================== Audio ====================

bool SQLiteDBAdapter::insert_audio_file(const AudioFileInfo& info) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] insert_audio_file() called before init()");
        return false;
    }

    Statement stmt;
    if (!stmt.prepare(db_, INSERT_AUDIO_SQL)) {
        return false;
    }
    bool bound = stmt.bind_text(1, info.title) && stmt.bind_text(2, info.file_path) &&
                 stmt.bind_uint(3, info.file_size) && stmt.bind_text(4, info.cover_path) &&
                 stmt.bind_text(5, info.artist) && stmt.bind_text(6, info.encoding_format) &&
                 stmt.bind_uint(7, info.duration) && stmt.bind_uint(8, info.bit_depth) &&
                 stmt.bind_uint(9, info.sample_rate) && stmt.bind_uint(10, info.bit_rate) &&
                 stmt.bind_uint(11, info.channels) && stmt.bind_text(12, info.channel_layout) &&
                 stmt.bind_text(13, info.lyrics_path);
    if (!bound) {
        return false;
    }
    if (stmt.step() != SQLITE_DONE) {
        spdlog::error("[DBAdapter] insert_audio_file() failed: {}", sqlite3_errmsg(db_));
        return false;
    }

    spdlog::debug("[DBAdapter] insert_audio_file('{}') ok, rowid = {} (not returned to caller)",
                  info.title, sqlite3_last_insert_rowid(db_));
    return true;
}

std::vector<AudioFileInfo> SQLiteDBAdapter::query_audio_files() {
    std::vector<AudioFileInfo> files;
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] query_audio_files() called before init()");
        return files;
    }

    Statement stmt;
    if (!stmt.prepare(db_, SELECT_AUDIO_ALL_SQL)) {
        return files;
    }

    for (;;) {
        int rc = stmt.step();
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            spdlog::error("[DBAdapter] query_audio_files() failed: {}", sqlite3_errmsg(db_));
            // 不返回半份数据：出错时整体丢弃
            files.clear();
            return files;
        }

        AudioFileInfo info;
        read_audio_row(stmt, info);
        files.push_back(std::move(info));
    }

    spdlog::debug("[DBAdapter] query_audio_files(): {} row(s)", files.size());
    return files;
}

std::optional<AudioFileInfo> SQLiteDBAdapter::query_audio_file_by_id(int64_t id) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] query_audio_file_by_id() called before init()");
        return std::nullopt;
    }

    Statement stmt;
    if (!stmt.prepare(db_, SELECT_AUDIO_BY_ID_SQL) || !stmt.bind_int(1, id)) {
        return std::nullopt;
    }

    int rc = stmt.step();
    if (rc == SQLITE_DONE) {
        spdlog::debug("[DBAdapter] query_audio_file_by_id({}): no row", id);
        return std::nullopt;
    }
    if (rc != SQLITE_ROW) {
        spdlog::error("[DBAdapter] query_audio_file_by_id() failed: {}", sqlite3_errmsg(db_));
        return std::nullopt;
    }

    AudioFileInfo info;
    read_audio_row(stmt, info);
    return info;
}

bool SQLiteDBAdapter::update_audio_file(const AudioFileInfo& info) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] update_audio_file() called before init()");
        return false;
    }

    Statement stmt;
    if (!stmt.prepare(db_, UPDATE_AUDIO_SQL)) {
        return false;
    }
    bool bound = stmt.bind_text(1, info.title) && stmt.bind_text(2, info.file_path) &&
                 stmt.bind_uint(3, info.file_size) && stmt.bind_text(4, info.cover_path) &&
                 stmt.bind_text(5, info.artist) && stmt.bind_text(6, info.encoding_format) &&
                 stmt.bind_uint(7, info.duration) && stmt.bind_uint(8, info.bit_depth) &&
                 stmt.bind_uint(9, info.sample_rate) && stmt.bind_uint(10, info.bit_rate) &&
                 stmt.bind_uint(11, info.channels) && stmt.bind_text(12, info.channel_layout) &&
                 stmt.bind_text(13, info.lyrics_path) && stmt.bind_int(14, info.id);
    if (!bound) {
        return false;
    }
    if (stmt.step() != SQLITE_DONE) {
        spdlog::error("[DBAdapter] update_audio_file() failed: {}", sqlite3_errmsg(db_));
        return false;
    }
    if (sqlite3_changes(db_) == 0) {
        spdlog::warn("[DBAdapter] update_audio_file(): no row with id={}", info.id);
        return false;
    }
    return true;
}

bool SQLiteDBAdapter::delete_audio_file(int64_t id) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] delete_audio_file() called before init()");
        return false;
    }

    Statement stmt;
    if (!stmt.prepare(db_, DELETE_AUDIO_SQL) || !stmt.bind_int(1, id)) {
        return false;
    }
    if (stmt.step() != SQLITE_DONE) {
        spdlog::error("[DBAdapter] delete_audio_file() failed: {}", sqlite3_errmsg(db_));
        return false;
    }
    if (sqlite3_changes(db_) == 0) {
        spdlog::warn("[DBAdapter] delete_audio_file(): no row with id={}", id);
        return false;
    }
    return true;
}

// ==================== Video ====================

bool SQLiteDBAdapter::insert_video_file(const VideoFileInfo& info) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] insert_video_file() called before init()");
        return false;
    }

    Statement stmt;
    if (!stmt.prepare(db_, INSERT_VIDEO_SQL)) {
        return false;
    }
    bool bound = stmt.bind_text(1, info.filename) && stmt.bind_text(2, info.file_path) &&
                 stmt.bind_uint(3, info.file_size) && stmt.bind_text(4, info.author) &&
                 stmt.bind_uint(5, info.resolution_width) &&
                 stmt.bind_uint(6, info.resolution_height) && stmt.bind_uint(7, info.color_depth) &&
                 stmt.bind_uint(8, info.duration) && stmt.bind_text(9, info.encoding_format) &&
                 stmt.bind_double(10, info.frame_rate) && stmt.bind_uint(11, info.bit_rate);
    if (!bound) {
        return false;
    }
    if (stmt.step() != SQLITE_DONE) {
        spdlog::error("[DBAdapter] insert_video_file() failed: {}", sqlite3_errmsg(db_));
        return false;
    }

    spdlog::debug("[DBAdapter] insert_video_file('{}') ok, rowid = {} (not returned to caller)",
                  info.filename, sqlite3_last_insert_rowid(db_));
    return true;
}

std::vector<VideoFileInfo> SQLiteDBAdapter::query_video_files() {
    std::vector<VideoFileInfo> files;
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] query_video_files() called before init()");
        return files;
    }

    Statement stmt;
    if (!stmt.prepare(db_, SELECT_VIDEO_ALL_SQL)) {
        return files;
    }

    for (;;) {
        int rc = stmt.step();
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            spdlog::error("[DBAdapter] query_video_files() failed: {}", sqlite3_errmsg(db_));
            files.clear();
            return files;
        }

        VideoFileInfo info;
        read_video_row(stmt, info);
        files.push_back(std::move(info));
    }

    spdlog::debug("[DBAdapter] query_video_files(): {} row(s)", files.size());
    return files;
}

std::optional<VideoFileInfo> SQLiteDBAdapter::query_video_file_by_id(int64_t id) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] query_video_file_by_id() called before init()");
        return std::nullopt;
    }

    Statement stmt;
    if (!stmt.prepare(db_, SELECT_VIDEO_BY_ID_SQL) || !stmt.bind_int(1, id)) {
        return std::nullopt;
    }

    int rc = stmt.step();
    if (rc == SQLITE_DONE) {
        spdlog::debug("[DBAdapter] query_video_file_by_id({}): no row", id);
        return std::nullopt;
    }
    if (rc != SQLITE_ROW) {
        spdlog::error("[DBAdapter] query_video_file_by_id() failed: {}", sqlite3_errmsg(db_));
        return std::nullopt;
    }

    VideoFileInfo info;
    read_video_row(stmt, info);
    return info;
}

bool SQLiteDBAdapter::update_video_file(const VideoFileInfo& info) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] update_video_file() called before init()");
        return false;
    }

    Statement stmt;
    if (!stmt.prepare(db_, UPDATE_VIDEO_SQL)) {
        return false;
    }
    bool bound = stmt.bind_text(1, info.filename) && stmt.bind_text(2, info.file_path) &&
                 stmt.bind_uint(3, info.file_size) && stmt.bind_text(4, info.author) &&
                 stmt.bind_uint(5, info.resolution_width) &&
                 stmt.bind_uint(6, info.resolution_height) && stmt.bind_uint(7, info.color_depth) &&
                 stmt.bind_uint(8, info.duration) && stmt.bind_text(9, info.encoding_format) &&
                 stmt.bind_double(10, info.frame_rate) && stmt.bind_uint(11, info.bit_rate) &&
                 stmt.bind_int(12, info.id);
    if (!bound) {
        return false;
    }
    if (stmt.step() != SQLITE_DONE) {
        spdlog::error("[DBAdapter] update_video_file() failed: {}", sqlite3_errmsg(db_));
        return false;
    }
    if (sqlite3_changes(db_) == 0) {
        spdlog::warn("[DBAdapter] update_video_file(): no row with id={}", info.id);
        return false;
    }
    return true;
}

bool SQLiteDBAdapter::delete_video_file(int64_t id) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] delete_video_file() called before init()");
        return false;
    }

    Statement stmt;
    if (!stmt.prepare(db_, DELETE_VIDEO_SQL) || !stmt.bind_int(1, id)) {
        return false;
    }
    if (stmt.step() != SQLITE_DONE) {
        spdlog::error("[DBAdapter] delete_video_file() failed: {}", sqlite3_errmsg(db_));
        return false;
    }
    if (sqlite3_changes(db_) == 0) {
        spdlog::warn("[DBAdapter] delete_video_file(): no row with id={}", id);
        return false;
    }
    return true;
}

// ==================== Settings ====================

std::optional<std::string> SQLiteDBAdapter::get_setting(const std::string& key) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] get_setting() called before init()");
        return std::nullopt;
    }

    Statement stmt;
    if (!stmt.prepare(db_, SELECT_SETTING_SQL) || !stmt.bind_text(1, key)) {
        return std::nullopt;
    }

    int rc = stmt.step();
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }
    if (rc != SQLITE_ROW) {
        spdlog::error("[DBAdapter] get_setting() failed: {}", sqlite3_errmsg(db_));
        return std::nullopt;
    }

    return stmt.column_text(0);
}

bool SQLiteDBAdapter::set_setting(const std::string& key, const std::string& value) {
    if (db_ == nullptr) {
        spdlog::error("[DBAdapter] set_setting() called before init()");
        return false;
    }

    Statement stmt;
    if (!stmt.prepare(db_, UPSERT_SETTING_SQL) || !stmt.bind_text(1, key) ||
        !stmt.bind_text(2, value)) {
        return false;
    }
    if (stmt.step() != SQLITE_DONE) {
        spdlog::error("[DBAdapter] set_setting() failed: {}", sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

} // namespace Prism::Adapter
