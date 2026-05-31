#pragma once
#include <string>
#include <cstdint>
#include <vector>

namespace Prism::persistence {

/**
 * @struct AudioRecord
 * @brief 音频数据库记录结构体（数据映射层）
 * 
 * 结构体成员说明：
 * 
 * - id            数据库主键ID（自增）
 * - file_path      音频文件完整路径
 * - format          音频格式：AAC、MP3、Opus、FLAC、PCM 等
 * - bit_rate        音频比特率（bps）
 * - sample_rate    音频采样率（Hz）
 * - channels        音频声道数：1单声道、2立体声、6为5.1声道、8为7.1声道
 * - duration        音频总时长（毫秒）
 * - create_time    记录创建时间戳（秒）
 */
struct AudioRecord {
    int64_t id = 0;
    std::string file_path;
    std::string format;
    int bit_rate = 0;
    int sample_rate = 0;
    int channels = 0;
    uint64_t duration = 0;
    uint64_t create_time = 0;
};

/**
 * @class AudioDatabase
 * @brief 音频SQLite数据库抽象接口（持久化层）
 * 
 * 提供音频记录的增删改查能力，存储音频文件元数据，供上层引擎层调用
 */
class AudioDatabase {
public:
    virtual ~AudioDatabase() = default;

    /**
     * @brief 初始化数据库并创建音频记录表
     * @param db_path SQLite数据库文件路径
     * @return 初始化成功返回 true，失败返回 false
     */
    virtual bool init(const std::string& db_path) = 0;

    /**
     * @brief 插入一条音频记录到数据库
     * @param record 待插入的音频记录结构体
     * @return 插入成功返回记录主键ID，失败返回 -1
     */
    virtual int64_t insert_record(const AudioRecord& record) = 0;

    /**
     * @brief 根据主键ID查询单条音频记录
     * @param id 音频记录主键ID
     * @return AudioRecord 查询到的音频记录结构体
     */
    virtual AudioRecord query_record_by_id(int64_t id) = 0;

    /**
     * @brief 查询数据库中所有音频记录
     * @return std::vector<AudioRecord> 所有音频记录的数组
     */
    virtual std::vector<AudioRecord> query_all_records() = 0;

    /**
     * @brief 更新数据库中指定音频记录
     * @param record 待更新的音频记录结构体（需包含有效主键ID）
     * @return 更新成功返回 true，失败返回 false
     */
    virtual bool update_record(const AudioRecord& record) = 0;

    /**
     * @brief 根据主键ID删除单条音频记录
     * @param id 音频记录主键ID
     * @return 删除成功返回 true，失败返回 false
     */
    virtual bool delete_record(int64_t id) = 0;
};

}