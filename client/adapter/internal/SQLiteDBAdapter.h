#pragma once

#include <string>

#include "DBAdapter.h"

// 只在头文件里前置声明，sqlite3.h 不外泄到包含本头文件的编译单元。
struct sqlite3;

namespace Prism::Adapter {

/**
 * @class SQLiteDBAdapter
 * @brief SQLite 数据库操作实现
 *
 * 连接用 `sqlite3_open_v2(READWRITE | CREATE | FULLMUTEX)` 打开：FULLMUTEX 让同一个
 * 连接可被多线程串行使用，因此类内不再额外加互斥量。所有 SQL 一律 `prepare_v2` +
 * 绑定参数，不拼接字符串。`init()` 在一个事务里建齐四张表（见 `设计文档.md` §7.1），
 * 任一步失败即回滚并关闭连接，不留半套 schema。
 */
class SQLiteDBAdapter : public DBAdapter {
  public:
    SQLiteDBAdapter() = default;
    ~SQLiteDBAdapter() override;

    // 持有 sqlite3* 的独占所有权，拷贝会让两个对象共享同一个句柄，直接禁掉。
    SQLiteDBAdapter(const SQLiteDBAdapter&) = delete;
    SQLiteDBAdapter& operator=(const SQLiteDBAdapter&) = delete;

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
    /**
     * @brief 检查连接是否可用，不可用时按调用点记一条 error 日志
     * @param what 调用方方法名，只用于日志
     * @return 连接可用返回 true
     */
    [[nodiscard]] bool ensure_open(const char* what) const;

    /**
     * @brief 建表：四张表在一个事务里完成
     * @return 成功返回 true
     */
    [[nodiscard]] bool ensure_schema();

    sqlite3* db_{nullptr}; /**< SQLite 连接句柄，未连接时为 nullptr */
    std::string db_path_;  /**< 已打开的数据库文件路径，仅用于日志 */
};

} // namespace Prism::Adapter
