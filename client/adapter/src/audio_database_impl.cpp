#include "../impl/audio_database_impl.h"
#include <spdlog/spdlog.h>
#include <ctime>

namespace Prism::persistence {

/**
 * @brief 执行SQL语句（内部私有方法）
 * @param sql 待执行的SQL语句
 * @return 执行成功返回 true，失败返回 false
 */
bool AudioDatabaseImpl::exec_sql(const std::string& sql) {
    char* err_msg = nullptr;
    int ret = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (ret != SQLITE_OK) {
        spdlog::error("sql error: {}", err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

/**
 * @brief 析构函数：关闭数据库连接、释放资源
 */
AudioDatabaseImpl::~AudioDatabaseImpl() {
    if (db_) {
        sqlite3_close(db_);
    }
}

/**
 * @brief 初始化数据库并创建音频记录表
 * @param db_path SQLite数据库文件路径
 * @return 初始化成功返回 true，失败返回 false
 */
bool AudioDatabaseImpl::init(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        spdlog::error("open db failed");
        return false;
    }

    const std::string create_table = R"(
        CREATE TABLE IF NOT EXISTS audio_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT, format TEXT, bit_rate INTEGER,
            sample_rate INTEGER, channels INTEGER, duration INTEGER, create_time INTEGER
        );
    )";
    return exec_sql(create_table);
}

/**
 * @brief 插入一条音频记录到数据库
 * @param record 待插入的音频记录结构体
 * @return 插入成功返回记录主键ID，失败返回 -1
 */
int64_t AudioDatabaseImpl::insert_record(const AudioRecord& record) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO audio_records VALUES(NULL,?,?,?,?,?,?,?)";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, record.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record.format.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, record.bit_rate);
    sqlite3_bind_int(stmt, 4, record.sample_rate);
    sqlite3_bind_int(stmt, 5, record.channels);
    sqlite3_bind_int64(stmt, 6, record.duration);
    sqlite3_bind_int64(stmt, 7, time(nullptr));

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return sqlite3_last_insert_rowid(db_);
}

/**
 * @brief 根据主键ID查询单条音频记录
 * @param id 音频记录主键ID
 * @return AudioRecord 查询到的音频记录结构体
 */
AudioRecord AudioDatabaseImpl::query_record_by_id(int64_t id) {
    AudioRecord record{};
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT * FROM audio_records WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        record.id = sqlite3_column_int64(stmt, 0);
        record.file_path = (const char*)sqlite3_column_text(stmt, 1);
        record.format = (const char*)sqlite3_column_text(stmt, 2);
        record.bit_rate = sqlite3_column_int(stmt, 3);
        record.sample_rate = sqlite3_column_int(stmt, 4);
        record.channels = sqlite3_column_int(stmt, 5);
        record.duration = sqlite3_column_int64(stmt, 6);
        record.create_time = sqlite3_column_int64(stmt, 7);
    }

    sqlite3_finalize(stmt);
    return record;
}

/**
 * @brief 查询数据库中所有音频记录
 * @return std::vector<AudioRecord> 所有音频记录的数组
 */
std::vector<AudioRecord> AudioDatabaseImpl::query_all_records() {
    std::vector<AudioRecord> res;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT * FROM audio_records", -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AudioRecord r{};
        r.id = sqlite3_column_int64(stmt, 0);
        r.file_path = (const char*)sqlite3_column_text(stmt, 1);
        r.format = (const char*)sqlite3_column_text(stmt, 2);
        r.bit_rate = sqlite3_column_int(stmt, 3);
        r.sample_rate = sqlite3_column_int(stmt, 4);
        r.channels = sqlite3_column_int(stmt, 5);
        r.duration = sqlite3_column_int64(stmt, 6);
        r.create_time = sqlite3_column_int64(stmt, 7);
        res.push_back(r);
    }

    sqlite3_finalize(stmt);
    return res;
}

/**
 * @brief 更新数据库中指定音频记录
 * @param record 待更新的音频记录结构体（需包含有效主键ID）
 * @return 更新成功返回 true，失败返回 false
 */
bool AudioDatabaseImpl::update_record(const AudioRecord& record) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"(
        UPDATE audio_records SET file_path=?,format=?,bit_rate=?,sample_rate=?,channels=?,duration=? WHERE id=?
    )";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, record.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record.format.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, record.bit_rate);
    sqlite3_bind_int(stmt, 4, record.sample_rate);
    sqlite3_bind_int(stmt, 5, record.channels);
    sqlite3_bind_int64(stmt, 6, record.duration);
    sqlite3_bind_int64(stmt, 7, record.id);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

/**
 * @brief 根据主键ID删除单条音频记录
 * @param id 音频记录主键ID
 * @return 删除成功返回 true，失败返回 false
 */
bool AudioDatabaseImpl::delete_record(int64_t id) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "DELETE FROM audio_records WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

}