#pragma once
#include "../include/audio_database.h"
#include <sqlite3.h>

namespace Prism::persistence {

/**
 * @class AudioDatabaseImpl
 * @brief 音频SQLite数据库接口的具体实现类
 * 
 * 基于SQLite实现音频记录的增删改查，管理音频文件元数据
 */
class AudioDatabaseImpl final : public AudioDatabase {
private:
    sqlite3* db_ = nullptr;

    /**
     * @brief 执行SQL语句（内部私有方法）
     * @param sql 待执行的SQL语句字符串
     * @return 执行成功返回 true，失败返回 false
     */
    bool exec_sql(const std::string& sql);

public:
    /**
     * @brief 析构函数：关闭数据库连接、释放资源
     */
    ~AudioDatabaseImpl() override;

    /**
     * @brief 初始化数据库并创建音频记录表（实现）
     */
    bool init(const std::string& db_path) override;

    /**
     * @brief 插入音频记录（实现）
     */
    int64_t insert_record(const AudioRecord& record) override;

    /**
     * @brief 按ID查询音频记录（实现）
     */
    AudioRecord query_record_by_id(int64_t id) override;

    /**
     * @brief 查询所有音频记录（实现）
     */
    std::vector<AudioRecord> query_all_records() override;

    /**
     * @brief 更新音频记录（实现）
     */
    bool update_record(const AudioRecord& record) override;

    /**
     * @brief 删除音频记录（实现）
     */
    bool delete_record(int64_t id) override;
};

}