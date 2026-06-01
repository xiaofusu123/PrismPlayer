#pragma once
#include "../include/AudioDatabase.h"
#include <sqlite3.h>

namespace Prism::Persistence {

/**
 * @class AudioDatabaseSQLite
 * @brief 音频数据库SQLite实现类
 *
 * 基于SQLite3实现音频数据库的增删改查功能
 * 封装数据库连接、SQL执行、资源释放逻辑
 */
class AudioDatabaseSQLite final : public AudioDatabase {
private:
    sqlite3* db_ = nullptr; /**< SQLite数据库连接句柄 */

    /**
     * @brief 内部辅助方法：执行SQL语句
     * @param sql 待执行的SQL语句字符串
     * @return 执行成功返回true，失败返回false
     */
    bool ExecSql(const std::string& sql);

public:
    /**
     * @brief 析构函数：关闭数据库连接、释放资源
     */
    ~AudioDatabaseSQLite() override;

    /**
     * @brief 初始化SQLite音频数据库
     */
    bool Init(const std::string& db_path) override;

    /**
     * @brief 新增音频记录（SQLite实现）
     */
    int64_t AddAudioRecord(const AudioFileRecord& record) override;

    /**
     * @brief 根据ID查询音频记录（SQLite实现）
     */
    AudioFileRecord GetAudioRecordById(int64_t id) override;

    /**
     * @brief 查询所有音频记录（SQLite实现）
     */
    std::vector<AudioFileRecord> GetAllAudioRecords() override;

    /**
     * @brief 更新音频记录（SQLite实现）
     */
    bool UpdateAudioRecord(const AudioFileRecord& record) override;

    /**
     * @brief 删除音频记录（SQLite实现）
     */
    bool DeleteAudioRecord(int64_t id) override;
};

} // namespace Prism::Persistence