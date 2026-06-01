#include "../Impl/AudioDatabaseSQLite.h"
#include <spdlog/spdlog.h>
#include <ctime>

namespace Prism::Persistence {

/**
 * @brief 内部辅助方法：执行SQL语句
 * @param sql 待执行的SQL语句
 * @return 执行成功返回true，失败返回false
 */
bool AudioDatabaseSQLite::ExecSql(const std::string& sql) {
    char* err = nullptr;
    int ret = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (ret != SQLITE_OK) {
        spdlog::error("SQL错误: {}", err);
        sqlite3_free(err);
        return false;
    }
    return true;
}

/**
 * @brief 析构函数：关闭数据库连接、释放资源
 */
AudioDatabaseSQLite::~AudioDatabaseSQLite() {
    if (db_) sqlite3_close(db_);
}

/**
 * @brief 初始化SQLite音频数据库，创建音频记录表
 * @param db_path SQLite数据库文件路径
 * @return 初始化成功返回true，失败返回false
 */
bool AudioDatabaseSQLite::Init(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK)
        return false;

    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS audio_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            song_name TEXT, file_path TEXT UNIQUE, file_size INTEGER,
            cover_path TEXT, composer TEXT, audio_format TEXT, duration INTEGER,
            bit_depth INTEGER, sample_rate INTEGER, bit_rate INTEGER,
            channel_count INTEGER, channel_layout TEXT, lyrics_path TEXT, create_time INTEGER
        );
    )";
    return ExecSql(sql);
}

/**
 * @brief 新增音频文件记录到SQLite数据库
 * @param r 待新增的音频文件记录结构体
 * @return 新增成功返回记录主键ID，失败返回-1
 */
int64_t AudioDatabaseSQLite::AddAudioRecord(const AudioFileRecord& r) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO audio_records VALUES(NULL,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt,1,r.song_name.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,r.file_path.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt,3,r.file_size);
    sqlite3_bind_text(stmt,4,r.cover_path.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,5,r.composer.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,6,r.audio_format.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt,7,r.duration);
    sqlite3_bind_int(stmt,8,r.bit_depth);
    sqlite3_bind_int(stmt,9,r.sample_rate);
    sqlite3_bind_int(stmt,10,r.bit_rate);
    sqlite3_bind_int(stmt,11,r.channel_count);
    sqlite3_bind_text(stmt,12,r.channel_layout.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,13,r.lyrics_path.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt,14,time(nullptr));

    sqlite3_step(stmt);
    int64_t id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return id;
}

/**
 * @brief 根据记录ID查询音频文件记录
 * @param id 音频记录主键ID
 * @return 查询到的音频记录，不存在则返回空结构体
 */
AudioFileRecord AudioDatabaseSQLite::GetAudioRecordById(int64_t id) {
    AudioFileRecord r{};
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT * FROM audio_records WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt,1,id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        r.id = sqlite3_column_int64(stmt,0);
        r.song_name = (const char*)sqlite3_column_text(stmt,1);
        r.file_path = (const char*)sqlite3_column_text(stmt,2);
        r.file_size = sqlite3_column_int64(stmt,3);
        r.cover_path = (const char*)sqlite3_column_text(stmt,4);
        r.composer = (const char*)sqlite3_column_text(stmt,5);
        r.audio_format = (const char*)sqlite3_column_text(stmt,6);
        r.duration = sqlite3_column_int64(stmt,7);
        r.bit_depth = sqlite3_column_int(stmt,8);
        r.sample_rate = sqlite3_column_int(stmt,9);
        r.bit_rate = sqlite3_column_int(stmt,10);
        r.channel_count = sqlite3_column_int(stmt,11);
        r.channel_layout = (const char*)sqlite3_column_text(stmt,12);
        r.lyrics_path = (const char*)sqlite3_column_text(stmt,13);
        r.create_time = sqlite3_column_int64(stmt,14);
    }
    sqlite3_finalize(stmt);
    return r;
}

/**
 * @brief 查询SQLite数据库中所有音频文件记录
 * @return 所有音频记录的数组列表
 */
std::vector<AudioFileRecord> AudioDatabaseSQLite::GetAllAudioRecords() {
    std::vector<AudioFileRecord> vec;
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT * FROM audio_records", -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AudioFileRecord r{};
        r.id = sqlite3_column_int64(stmt,0);
        r.song_name = (const char*)sqlite3_column_text(stmt,1);
        r.file_path = (const char*)sqlite3_column_text(stmt,2);
        r.file_size = sqlite3_column_int64(stmt,3);
        r.cover_path = (const char*)sqlite3_column_text(stmt,4);
        r.composer = (const char*)sqlite3_column_text(stmt,5);
        r.audio_format = (const char*)sqlite3_column_text(stmt,6);
        r.duration = sqlite3_column_int64(stmt,7);
        r.bit_depth = sqlite3_column_int(stmt,8);
        r.sample_rate = sqlite3_column_int(stmt,9);
        r.bit_rate = sqlite3_column_int(stmt,10);
        r.channel_count = sqlite3_column_int(stmt,11);
        r.channel_layout = (const char*)sqlite3_column_text(stmt,12);
        r.lyrics_path = (const char*)sqlite3_column_text(stmt,13);
        r.create_time = sqlite3_column_int64(stmt,14);
        vec.push_back(r);
    }
    sqlite3_finalize(stmt);
    return vec;
}

/**
 * @brief 更新SQLite数据库中的音频文件记录
 * @param r 待更新的音频记录（需包含有效主键ID）
 * @return 更新成功返回true，失败返回false
 */
bool AudioDatabaseSQLite::UpdateAudioRecord(const AudioFileRecord& r) {
    sqlite3_stmt* stmt;
    const char* sql = R"(UPDATE audio_records SET song_name=?,file_path=?,file_size=?,cover_path=?,composer=?,
    audio_format=?,duration=?,bit_depth=?,sample_rate=?,bit_rate=?,channel_count=?,channel_layout=?,lyrics_path=? WHERE id=?)";

    sqlite3_prepare_v2(db_,sql,-1,&stmt,nullptr);
    sqlite3_bind_text(stmt,1,r.song_name.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,r.file_path.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt,3,r.file_size);
    sqlite3_bind_text(stmt,4,r.cover_path.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,5,r.composer.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,6,r.audio_format.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt,7,r.duration);
    sqlite3_bind_int(stmt,8,r.bit_depth);
    sqlite3_bind_int(stmt,9,r.sample_rate);
    sqlite3_bind_int(stmt,10,r.bit_rate);
    sqlite3_bind_int(stmt,11,r.channel_count);
    sqlite3_bind_text(stmt,12,r.channel_layout.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,13,r.lyrics_path.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt,14,r.id);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

/**
 * @brief 根据记录ID删除音频文件记录
 * @param id 音频记录主键ID
 * @return 删除成功返回true，失败返回false
 */
bool AudioDatabaseSQLite::DeleteAudioRecord(int64_t id) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "DELETE FROM audio_records WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt,1,id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

} // namespace Prism::Persistence