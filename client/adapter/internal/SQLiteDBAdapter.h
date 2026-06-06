#pragma once

#include "DBAdapter.h"

namespace Prism::Adapter {

/**
 * @class SQLiteDBAdapter
 * @brief SQLite 数据库操作实现
 */
class SQLiteDBAdapter : public DBAdapter {
public:
    SQLiteDBAdapter() = default;
    ~SQLiteDBAdapter() override = default;

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
    // TODO: 添加 sqlite3* 数据库句柄等私有成员
};

} // namespace Prism::Adapter
