#include "SQLiteDBAdapter.h"

#include <sqlite3.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

namespace Prism::Adapter {
namespace {

/** @brief 等锁超时：另一个进程/线程正在写时最多等这么久，避免直接报 SQLITE_BUSY */
constexpr int BUSY_TIMEOUT_MS = 5000;

/**
 * @brief 建表语句：与 `docs/markdown/设计文档.md` §7.1 的四张表一一对应
 *
 * 整段包在一个事务里：中途任一句失败就回滚，不留半套 schema。
 */
constexpr const char* SCHEMA_SQL = R"SQL(
BEGIN;
CREATE TABLE IF NOT EXISTS accounts (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT    NOT NULL UNIQUE,
    email         TEXT    NOT NULL,
    password_hash TEXT    NOT NULL
);
CREATE TABLE IF NOT EXISTS audio_files (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    title           TEXT    NOT NULL,
    file_path       TEXT    NOT NULL,
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
);
CREATE TABLE IF NOT EXISTS video_files (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    filename          TEXT    NOT NULL,
    file_path         TEXT    NOT NULL,
    file_size         INTEGER NOT NULL DEFAULT 0,
    author            TEXT    NOT NULL DEFAULT '',
    resolution_width  INTEGER NOT NULL DEFAULT 0,
    resolution_height INTEGER NOT NULL DEFAULT 0,
    color_depth       INTEGER NOT NULL DEFAULT 0,
    duration          INTEGER NOT NULL DEFAULT 0,
    encoding_format   TEXT    NOT NULL DEFAULT '',
    frame_rate        REAL    NOT NULL DEFAULT 0,
    bit_rate          INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS settings (
    "key"   TEXT PRIMARY KEY,
    "value" TEXT NOT NULL
);
COMMIT;
)SQL";

// ==================== accounts ====================

constexpr const char* ACCOUNT_INSERT_SQL =
    "INSERT INTO accounts(username, email, password_hash) VALUES(?1, ?2, ?3);";
constexpr const char* ACCOUNT_SELECT_BY_USERNAME_SQL =
    "SELECT id, username, email, password_hash FROM accounts WHERE username = ?1 LIMIT 1;";
constexpr const char* ACCOUNT_UPDATE_SQL =
    "UPDATE accounts SET username = ?1, email = ?2, password_hash = ?3 WHERE id = ?4;";
constexpr const char* ACCOUNT_DELETE_SQL = "DELETE FROM accounts WHERE id = ?1;";

// ==================== audio_files ====================

constexpr const char* AUDIO_INSERT_SQL =
    "INSERT INTO audio_files(title, file_path, file_size, cover_path, artist, encoding_format, "
    "duration, bit_depth, sample_rate, bit_rate, channels, channel_layout, lyrics_path) "
    "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13);";
constexpr const char* AUDIO_SELECT_ALL_SQL =
    "SELECT id, title, file_path, file_size, cover_path, artist, encoding_format, duration, "
    "bit_depth, sample_rate, bit_rate, channels, channel_layout, lyrics_path "
    "FROM audio_files ORDER BY id;";
constexpr const char* AUDIO_SELECT_BY_ID_SQL =
    "SELECT id, title, file_path, file_size, cover_path, artist, encoding_format, duration, "
    "bit_depth, sample_rate, bit_rate, channels, channel_layout, lyrics_path "
    "FROM audio_files WHERE id = ?1 LIMIT 1;";
constexpr const char* AUDIO_UPDATE_SQL =
    "UPDATE audio_files SET title = ?1, file_path = ?2, file_size = ?3, cover_path = ?4, "
    "artist = ?5, encoding_format = ?6, duration = ?7, bit_depth = ?8, sample_rate = ?9, "
    "bit_rate = ?10, channels = ?11, channel_layout = ?12, lyrics_path = ?13 WHERE id = ?14;";
constexpr const char* AUDIO_DELETE_SQL = "DELETE FROM audio_files WHERE id = ?1;";

// ==================== video_files ====================

constexpr const char* VIDEO_INSERT_SQL =
    "INSERT INTO video_files(filename, file_path, file_size, author, resolution_width, "
    "resolution_height, color_depth, duration, encoding_format, frame_rate, bit_rate) "
    "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11);";
constexpr const char* VIDEO_SELECT_ALL_SQL =
    "SELECT id, filename, file_path, file_size, author, resolution_width, resolution_height, "
    "color_depth, duration, encoding_format, frame_rate, bit_rate FROM video_files ORDER BY id;";
constexpr const char* VIDEO_SELECT_BY_ID_SQL =
    "SELECT id, filename, file_path, file_size, author, resolution_width, resolution_height, "
    "color_depth, duration, encoding_format, frame_rate, bit_rate "
    "FROM video_files WHERE id = ?1 LIMIT 1;";
constexpr const char* VIDEO_UPDATE_SQL =
    "UPDATE video_files SET filename = ?1, file_path = ?2, file_size = ?3, author = ?4, "
    "resolution_width = ?5, resolution_height = ?6, color_depth = ?7, duration = ?8, "
    "encoding_format = ?9, frame_rate = ?10, bit_rate = ?11 WHERE id = ?12;";
constexpr const char* VIDEO_DELETE_SQL = "DELETE FROM video_files WHERE id = ?1;";

// ==================== settings ====================

constexpr const char* SETTING_SELECT_SQL =
    "SELECT \"value\" FROM settings WHERE \"key\" = ?1 LIMIT 1;";
constexpr const char* SETTING_UPSERT_SQL =
    "INSERT INTO settings(\"key\", \"value\") VALUES(?1, ?2) "
    "ON CONFLICT(\"key\") DO UPDATE SET \"value\" = excluded.\"value\";";

// SELECT 的列顺序，与上面每条 SQL 的列清单严格对应：改列清单必须同步改这里。
constexpr int ACCOUNT_COL_ID = 0;
constexpr int ACCOUNT_COL_USERNAME = 1;
constexpr int ACCOUNT_COL_EMAIL = 2;
constexpr int ACCOUNT_COL_PASSWORD_HASH = 3;

constexpr int AUDIO_COL_ID = 0;
constexpr int AUDIO_COL_TITLE = 1;
constexpr int AUDIO_COL_FILE_PATH = 2;
constexpr int AUDIO_COL_FILE_SIZE = 3;
constexpr int AUDIO_COL_COVER_PATH = 4;
constexpr int AUDIO_COL_ARTIST = 5;
constexpr int AUDIO_COL_ENCODING_FORMAT = 6;
constexpr int AUDIO_COL_DURATION = 7;
constexpr int AUDIO_COL_BIT_DEPTH = 8;
constexpr int AUDIO_COL_SAMPLE_RATE = 9;
constexpr int AUDIO_COL_BIT_RATE = 10;
constexpr int AUDIO_COL_CHANNELS = 11;
constexpr int AUDIO_COL_CHANNEL_LAYOUT = 12;
constexpr int AUDIO_COL_LYRICS_PATH = 13;

constexpr int VIDEO_COL_ID = 0;
constexpr int VIDEO_COL_FILENAME = 1;
constexpr int VIDEO_COL_FILE_PATH = 2;
constexpr int VIDEO_COL_FILE_SIZE = 3;
constexpr int VIDEO_COL_AUTHOR = 4;
constexpr int VIDEO_COL_RESOLUTION_WIDTH = 5;
constexpr int VIDEO_COL_RESOLUTION_HEIGHT = 6;
constexpr int VIDEO_COL_COLOR_DEPTH = 7;
constexpr int VIDEO_COL_DURATION = 8;
constexpr int VIDEO_COL_ENCODING_FORMAT = 9;
constexpr int VIDEO_COL_FRAME_RATE = 10;
constexpr int VIDEO_COL_BIT_RATE = 11;

/**
 * @brief `sqlite3_stmt` 的 RAII 包装，析构时调用 `sqlite3_finalize`
 */
using StatementPtr = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

/**
 * @brief 编译一条 SQL
 * @param db 数据库连接
 * @param sql SQL 文本
 * @return 成功返回持有语句的智能指针，失败返回空指针（已记日志）
 */
[[nodiscard]] StatementPtr prepare(sqlite3* db, const char* sql) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) {
        spdlog::error("[DBAdapter] prepare failed: {} (sql: {})", sqlite3_errmsg(db), sql);
        return StatementPtr(nullptr, &sqlite3_finalize);
    }
    return StatementPtr(raw, &sqlite3_finalize);
}

/**
 * @brief 绑定一个文本参数
 * @param stmt 目标语句
 * @param index 参数下标（从 1 开始）
 * @param value 文本值
 * @return 成功返回 true
 */
[[nodiscard]] bool bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
    // SQLITE_TRANSIENT：让 SQLite 立刻拷贝一份，调用方不必保证 value 的生命周期。
    return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

/**
 * @brief 绑定一个整数参数
 * @param stmt 目标语句
 * @param index 参数下标（从 1 开始）
 * @param value 整数值
 * @return 成功返回 true
 */
[[nodiscard]] bool bind_int(sqlite3_stmt* stmt, int index, int64_t value) {
    return sqlite3_bind_int64(stmt, index, static_cast<sqlite3_int64>(value)) == SQLITE_OK;
}

/**
 * @brief 绑定一个浮点参数
 * @param stmt 目标语句
 * @param index 参数下标（从 1 开始）
 * @param value 浮点值
 * @return 成功返回 true
 */
[[nodiscard]] bool bind_double(sqlite3_stmt* stmt, int index, double value) {
    return sqlite3_bind_double(stmt, index, value) == SQLITE_OK;
}

/**
 * @brief 读一列文本；列值为 NULL 时返回空串
 * @param stmt 已定位到一行的语句
 * @param index 列下标（从 0 开始）
 * @return 列文本
 */
std::string column_text(sqlite3_stmt* stmt, int index) {
    const unsigned char* text = sqlite3_column_text(stmt, index);
    if (text == nullptr) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(text));
}

[[nodiscard]] uint64_t column_u64(sqlite3_stmt* stmt, int index) {
    auto value = sqlite3_column_int64(stmt, index);
    return value < 0 ? 0 : static_cast<uint64_t>(value);
}

[[nodiscard]] uint32_t column_u32(sqlite3_stmt* stmt, int index) {
    auto value = sqlite3_column_int64(stmt, index);
    return value < 0 ? 0 : static_cast<uint32_t>(value);
}

[[nodiscard]] AccountInfo read_account(sqlite3_stmt* stmt) {
    AccountInfo info;
    info.id = sqlite3_column_int64(stmt, ACCOUNT_COL_ID);
    info.username = column_text(stmt, ACCOUNT_COL_USERNAME);
    info.email = column_text(stmt, ACCOUNT_COL_EMAIL);
    info.password_hash = column_text(stmt, ACCOUNT_COL_PASSWORD_HASH);
    return info;
}

[[nodiscard]] AudioFileInfo read_audio_file(sqlite3_stmt* stmt) {
    AudioFileInfo info;
    info.id = sqlite3_column_int64(stmt, AUDIO_COL_ID);
    info.title = column_text(stmt, AUDIO_COL_TITLE);
    info.file_path = column_text(stmt, AUDIO_COL_FILE_PATH);
    info.file_size = column_u64(stmt, AUDIO_COL_FILE_SIZE);
    info.cover_path = column_text(stmt, AUDIO_COL_COVER_PATH);
    info.artist = column_text(stmt, AUDIO_COL_ARTIST);
    info.encoding_format = column_text(stmt, AUDIO_COL_ENCODING_FORMAT);
    info.duration = column_u64(stmt, AUDIO_COL_DURATION);
    info.bit_depth = column_u32(stmt, AUDIO_COL_BIT_DEPTH);
    info.sample_rate = column_u32(stmt, AUDIO_COL_SAMPLE_RATE);
    info.bit_rate = column_u64(stmt, AUDIO_COL_BIT_RATE);
    info.channels = column_u32(stmt, AUDIO_COL_CHANNELS);
    info.channel_layout = column_text(stmt, AUDIO_COL_CHANNEL_LAYOUT);
    info.lyrics_path = column_text(stmt, AUDIO_COL_LYRICS_PATH);
    return info;
}

[[nodiscard]] VideoFileInfo read_video_file(sqlite3_stmt* stmt) {
    VideoFileInfo info;
    info.id = sqlite3_column_int64(stmt, VIDEO_COL_ID);
    info.filename = column_text(stmt, VIDEO_COL_FILENAME);
    info.file_path = column_text(stmt, VIDEO_COL_FILE_PATH);
    info.file_size = column_u64(stmt, VIDEO_COL_FILE_SIZE);
    info.author = column_text(stmt, VIDEO_COL_AUTHOR);
    info.resolution_width = column_u32(stmt, VIDEO_COL_RESOLUTION_WIDTH);
    info.resolution_height = column_u32(stmt, VIDEO_COL_RESOLUTION_HEIGHT);
    info.color_depth = column_u32(stmt, VIDEO_COL_COLOR_DEPTH);
    info.duration = column_u64(stmt, VIDEO_COL_DURATION);
    info.encoding_format = column_text(stmt, VIDEO_COL_ENCODING_FORMAT);
    info.frame_rate = sqlite3_column_double(stmt, VIDEO_COL_FRAME_RATE);
    info.bit_rate = column_u64(stmt, VIDEO_COL_BIT_RATE);
    return info;
}

/**
 * @brief 执行一条已绑定的写语句（INSERT / UPDATE / DELETE）
 * @param db 数据库连接，仅用于取错误信息
 * @param stmt 已绑定参数的语句
 * @param what 调用方方法名，用于日志
 * @return 执行成功且至少影响一行返回 true
 */
[[nodiscard]] bool run_write(sqlite3* db, sqlite3_stmt* stmt, const char* what) {
    if (stmt == nullptr) {
        return false;
    }

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        spdlog::error("[DBAdapter] {} failed: {} (code {})", what, sqlite3_errmsg(db), rc);
        return false;
    }

    if (sqlite3_changes(db) <= 0) {
        spdlog::debug("[DBAdapter] {} affected no rows", what);
        return false;
    }
    return true;
}

// 音频 / 视频字段的绑定集中在一处：insert 与 update 的列清单一致，只有 update 多一个 id。
[[nodiscard]] bool bind_audio_fields(sqlite3_stmt* stmt, const AudioFileInfo& info) {
    return bind_text(stmt, 1, info.title) && bind_text(stmt, 2, info.file_path) &&
           bind_int(stmt, 3, static_cast<int64_t>(info.file_size)) &&
           bind_text(stmt, 4, info.cover_path) && bind_text(stmt, 5, info.artist) &&
           bind_text(stmt, 6, info.encoding_format) &&
           bind_int(stmt, 7, static_cast<int64_t>(info.duration)) &&
           bind_int(stmt, 8, static_cast<int64_t>(info.bit_depth)) &&
           bind_int(stmt, 9, static_cast<int64_t>(info.sample_rate)) &&
           bind_int(stmt, 10, static_cast<int64_t>(info.bit_rate)) &&
           bind_int(stmt, 11, static_cast<int64_t>(info.channels)) &&
           bind_text(stmt, 12, info.channel_layout) && bind_text(stmt, 13, info.lyrics_path);
}

[[nodiscard]] bool bind_video_fields(sqlite3_stmt* stmt, const VideoFileInfo& info) {
    return bind_text(stmt, 1, info.filename) && bind_text(stmt, 2, info.file_path) &&
           bind_int(stmt, 3, static_cast<int64_t>(info.file_size)) &&
           bind_text(stmt, 4, info.author) &&
           bind_int(stmt, 5, static_cast<int64_t>(info.resolution_width)) &&
           bind_int(stmt, 6, static_cast<int64_t>(info.resolution_height)) &&
           bind_int(stmt, 7, static_cast<int64_t>(info.color_depth)) &&
           bind_int(stmt, 8, static_cast<int64_t>(info.duration)) &&
           bind_text(stmt, 9, info.encoding_format) && bind_double(stmt, 10, info.frame_rate) &&
           bind_int(stmt, 11, static_cast<int64_t>(info.bit_rate));
}

} // namespace

SQLiteDBAdapter::~SQLiteDBAdapter() {
    close();
}

bool SQLiteDBAdapter::ensure_open(const char* what) const {
    if (db_ != nullptr) {
        return true;
    }

    spdlog::error("[DBAdapter] {} called before init()", what);
    return false;
}

bool SQLiteDBAdapter::ensure_schema() {
    char* error_message = nullptr;
    if (sqlite3_exec(db_, SCHEMA_SQL, nullptr, nullptr, &error_message) != SQLITE_OK) {
        spdlog::error("[DBAdapter] cannot create schema: {}",
                      error_message == nullptr ? "unknown error" : error_message);
        sqlite3_free(error_message);
        // 事务已由脚本里的 BEGIN 打开，失败后显式回滚，避免连接停在未提交状态。
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    return true;
}

bool SQLiteDBAdapter::init(const std::string& db_path) {
    if (db_path.empty()) {
        spdlog::error("[DBAdapter] init() called with an empty path");
        return false;
    }

    close();

    sqlite3* handle = nullptr;
    int rc = sqlite3_open_v2(db_path.c_str(), &handle,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("[DBAdapter] cannot open '{}': {}", db_path,
                      handle == nullptr ? sqlite3_errstr(rc) : sqlite3_errmsg(handle));
        sqlite3_close_v2(handle);
        return false;
    }

    db_ = handle;
    db_path_ = db_path;
    sqlite3_busy_timeout(db_, BUSY_TIMEOUT_MS);

    if (!ensure_schema()) {
        close();
        return false;
    }

    spdlog::info("[DBAdapter] opened '{}'", db_path_);
    return true;
}

void SQLiteDBAdapter::close() {
    if (db_ == nullptr) {
        return;
    }

    // close_v2 允许仍有未 finalize 的语句（这里没有），比 close 更宽松。
    if (sqlite3_close_v2(db_) != SQLITE_OK) {
        spdlog::warn("[DBAdapter] cannot close '{}' cleanly", db_path_);
    } else if (!db_path_.empty()) {
        spdlog::debug("[DBAdapter] closed '{}'", db_path_);
    }

    db_ = nullptr;
    db_path_.clear();
}

// ==================== Account 账户操作 ====================

bool SQLiteDBAdapter::insert_account(const AccountInfo& info) {
    if (!ensure_open("insert_account")) {
        return false;
    }

    StatementPtr stmt = prepare(db_, ACCOUNT_INSERT_SQL);
    if (!stmt) {
        return false;
    }

    if (!bind_text(stmt.get(), 1, info.username) || !bind_text(stmt.get(), 2, info.email) ||
        !bind_text(stmt.get(), 3, info.password_hash)) {
        spdlog::error("[DBAdapter] insert_account failed to bind parameters");
        return false;
    }

    return run_write(db_, stmt.get(), "insert_account");
}

std::optional<AccountInfo> SQLiteDBAdapter::query_account_by_username(const std::string& username) {
    if (!ensure_open("query_account_by_username")) {
        return std::nullopt;
    }

    StatementPtr stmt = prepare(db_, ACCOUNT_SELECT_BY_USERNAME_SQL);
    if (!stmt || !bind_text(stmt.get(), 1, username)) {
        return std::nullopt;
    }

    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        return read_account(stmt.get());
    }
    if (rc != SQLITE_DONE) {
        spdlog::error("[DBAdapter] query_account_by_username failed: {}", sqlite3_errmsg(db_));
    }
    return std::nullopt;
}

bool SQLiteDBAdapter::update_account(const AccountInfo& info) {
    if (!ensure_open("update_account")) {
        return false;
    }

    StatementPtr stmt = prepare(db_, ACCOUNT_UPDATE_SQL);
    if (!stmt) {
        return false;
    }

    if (!bind_text(stmt.get(), 1, info.username) || !bind_text(stmt.get(), 2, info.email) ||
        !bind_text(stmt.get(), 3, info.password_hash) || !bind_int(stmt.get(), 4, info.id)) {
        spdlog::error("[DBAdapter] update_account failed to bind parameters");
        return false;
    }

    return run_write(db_, stmt.get(), "update_account");
}

bool SQLiteDBAdapter::delete_account(int64_t id) {
    if (!ensure_open("delete_account")) {
        return false;
    }

    StatementPtr stmt = prepare(db_, ACCOUNT_DELETE_SQL);
    if (!stmt || !bind_int(stmt.get(), 1, id)) {
        return false;
    }

    return run_write(db_, stmt.get(), "delete_account");
}

// ==================== Audio 音频文件操作 ====================

bool SQLiteDBAdapter::insert_audio_file(const AudioFileInfo& info) {
    if (!ensure_open("insert_audio_file")) {
        return false;
    }

    StatementPtr stmt = prepare(db_, AUDIO_INSERT_SQL);
    if (!stmt) {
        return false;
    }

    if (!bind_audio_fields(stmt.get(), info)) {
        spdlog::error("[DBAdapter] insert_audio_file failed to bind parameters");
        return false;
    }

    return run_write(db_, stmt.get(), "insert_audio_file");
}

std::vector<AudioFileInfo> SQLiteDBAdapter::query_audio_files() {
    std::vector<AudioFileInfo> files;
    if (!ensure_open("query_audio_files")) {
        return files;
    }

    StatementPtr stmt = prepare(db_, AUDIO_SELECT_ALL_SQL);
    if (!stmt) {
        return files;
    }

    int rc = sqlite3_step(stmt.get());
    while (rc == SQLITE_ROW) {
        files.push_back(read_audio_file(stmt.get()));
        rc = sqlite3_step(stmt.get());
    }

    if (rc != SQLITE_DONE) {
        spdlog::error("[DBAdapter] query_audio_files failed: {}", sqlite3_errmsg(db_));
    }
    return files;
}

std::optional<AudioFileInfo> SQLiteDBAdapter::query_audio_file_by_id(int64_t id) {
    if (!ensure_open("query_audio_file_by_id")) {
        return std::nullopt;
    }

    StatementPtr stmt = prepare(db_, AUDIO_SELECT_BY_ID_SQL);
    if (!stmt || !bind_int(stmt.get(), 1, id)) {
        return std::nullopt;
    }

    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        return read_audio_file(stmt.get());
    }
    if (rc != SQLITE_DONE) {
        spdlog::error("[DBAdapter] query_audio_file_by_id failed: {}", sqlite3_errmsg(db_));
    }
    return std::nullopt;
}

bool SQLiteDBAdapter::update_audio_file(const AudioFileInfo& info) {
    if (!ensure_open("update_audio_file")) {
        return false;
    }

    StatementPtr stmt = prepare(db_, AUDIO_UPDATE_SQL);
    if (!stmt) {
        return false;
    }

    if (!bind_audio_fields(stmt.get(), info) || !bind_int(stmt.get(), 14, info.id)) {
        spdlog::error("[DBAdapter] update_audio_file failed to bind parameters");
        return false;
    }

    return run_write(db_, stmt.get(), "update_audio_file");
}

bool SQLiteDBAdapter::delete_audio_file(int64_t id) {
    if (!ensure_open("delete_audio_file")) {
        return false;
    }

    StatementPtr stmt = prepare(db_, AUDIO_DELETE_SQL);
    if (!stmt || !bind_int(stmt.get(), 1, id)) {
        return false;
    }

    return run_write(db_, stmt.get(), "delete_audio_file");
}

// ==================== Video 视频文件操作 ====================

bool SQLiteDBAdapter::insert_video_file(const VideoFileInfo& info) {
    if (!ensure_open("insert_video_file")) {
        return false;
    }

    StatementPtr stmt = prepare(db_, VIDEO_INSERT_SQL);
    if (!stmt) {
        return false;
    }

    if (!bind_video_fields(stmt.get(), info)) {
        spdlog::error("[DBAdapter] insert_video_file failed to bind parameters");
        return false;
    }

    return run_write(db_, stmt.get(), "insert_video_file");
}

std::vector<VideoFileInfo> SQLiteDBAdapter::query_video_files() {
    std::vector<VideoFileInfo> files;
    if (!ensure_open("query_video_files")) {
        return files;
    }

    StatementPtr stmt = prepare(db_, VIDEO_SELECT_ALL_SQL);
    if (!stmt) {
        return files;
    }

    int rc = sqlite3_step(stmt.get());
    while (rc == SQLITE_ROW) {
        files.push_back(read_video_file(stmt.get()));
        rc = sqlite3_step(stmt.get());
    }

    if (rc != SQLITE_DONE) {
        spdlog::error("[DBAdapter] query_video_files failed: {}", sqlite3_errmsg(db_));
    }
    return files;
}

std::optional<VideoFileInfo> SQLiteDBAdapter::query_video_file_by_id(int64_t id) {
    if (!ensure_open("query_video_file_by_id")) {
        return std::nullopt;
    }

    StatementPtr stmt = prepare(db_, VIDEO_SELECT_BY_ID_SQL);
    if (!stmt || !bind_int(stmt.get(), 1, id)) {
        return std::nullopt;
    }

    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        return read_video_file(stmt.get());
    }
    if (rc != SQLITE_DONE) {
        spdlog::error("[DBAdapter] query_video_file_by_id failed: {}", sqlite3_errmsg(db_));
    }
    return std::nullopt;
}

bool SQLiteDBAdapter::update_video_file(const VideoFileInfo& info) {
    if (!ensure_open("update_video_file")) {
        return false;
    }

    StatementPtr stmt = prepare(db_, VIDEO_UPDATE_SQL);
    if (!stmt) {
        return false;
    }

    if (!bind_video_fields(stmt.get(), info) || !bind_int(stmt.get(), 12, info.id)) {
        spdlog::error("[DBAdapter] update_video_file failed to bind parameters");
        return false;
    }

    return run_write(db_, stmt.get(), "update_video_file");
}

bool SQLiteDBAdapter::delete_video_file(int64_t id) {
    if (!ensure_open("delete_video_file")) {
        return false;
    }

    StatementPtr stmt = prepare(db_, VIDEO_DELETE_SQL);
    if (!stmt || !bind_int(stmt.get(), 1, id)) {
        return false;
    }

    return run_write(db_, stmt.get(), "delete_video_file");
}

// ==================== Settings 设置操作 ====================

std::optional<std::string> SQLiteDBAdapter::get_setting(const std::string& key) {
    if (!ensure_open("get_setting")) {
        return std::nullopt;
    }

    StatementPtr stmt = prepare(db_, SETTING_SELECT_SQL);
    if (!stmt || !bind_text(stmt.get(), 1, key)) {
        return std::nullopt;
    }

    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        return column_text(stmt.get(), 0);
    }
    if (rc != SQLITE_DONE) {
        spdlog::error("[DBAdapter] get_setting failed: {}", sqlite3_errmsg(db_));
    }
    return std::nullopt;
}

bool SQLiteDBAdapter::set_setting(const std::string& key, const std::string& value) {
    if (!ensure_open("set_setting")) {
        return false;
    }

    StatementPtr stmt = prepare(db_, SETTING_UPSERT_SQL);
    if (!stmt) {
        return false;
    }

    if (!bind_text(stmt.get(), 1, key) || !bind_text(stmt.get(), 2, value)) {
        spdlog::error("[DBAdapter] set_setting failed to bind parameters");
        return false;
    }

    // UPSERT 在值未变化时 changes() 可能为 0，这里只看 step 是否成功。
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        spdlog::error("[DBAdapter] set_setting failed: {}", sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

} // namespace Prism::Adapter
