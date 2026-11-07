#pragma once

#include "DBAdapter.h"

#include <sqlite3.h>

namespace Prism::Adapter {

/**
 * @class SQLiteDBAdapter
 * @brief 基于 SQLite C API 的数据库操作实现
 *
 * - 连接用 `sqlite3_open_v2(READWRITE|CREATE|FULLMUTEX)`，单连接可被多线程串行使用，
 *   因此类内**不再加互斥量**。
 * - `init()` 在一个事务里建 4 张表（`accounts` / `audio_files` / `video_files` / `settings`），
 *   任一步失败即回滚并关闭连接，**不留半套 schema**。
 * - `password_hash` 在本层是**不透明字符串**：不参与哈希、不解析、不校验。
 *   具体格式由 `OpenSSLCryptoAdapter` 决定（`pbkdf2-sha256$<iter>$<salt>$<dk>`）。
 * - `insert_*` 接收 const 引用，**不回填 id**；需要 id 时用 `query_account_by_username()`
 *   或 `query_audio_files()` 查回。真要直接拿 id 属于改公共接口，需先设计。
 */
class SQLiteDBAdapter : public DBAdapter {
  public:
    SQLiteDBAdapter() = default;
    ~SQLiteDBAdapter() override;

    bool init(const std::string& db_path) override;
    void close() override;

    bool insert_account(const AccountInfo& info) override;
    std::optional<AccountInfo> query_account_by_username(const std::string& username) override;
    bool update_account(const AccountInfo& info) override;
    bool delete_account(int64_t id) override;

    bool insert_audio_file(const AudioFileInfo& info) override;
    std::vector<AudioFileInfo> query_audio_files() override;
    std::optional<AudioFileInfo> query_audio_file_by_id(int64_t id) override;
    bool update_audio_file(const AudioFileInfo& info) override;
    bool delete_audio_file(int64_t id) override;

    bool insert_video_file(const VideoFileInfo& info) override;
    std::vector<VideoFileInfo> query_video_files() override;
    std::optional<VideoFileInfo> query_video_file_by_id(int64_t id) override;
    bool update_video_file(const VideoFileInfo& info) override;
    bool delete_video_file(int64_t id) override;

    std::optional<std::string> get_setting(const std::string& key) override;
    bool set_setting(const std::string& key, const std::string& value) override;

  private:
    sqlite3* db_{nullptr}; /**< 数据库句柄，未 init() 或 close() 之后为空 */
};

} // namespace Prism::Adapter
