#pragma once
#include <string>
#include <cstdint>
#include <vector>

namespace Prism::Persistence {

/**
 * @struct AudioFileRecord
 * @brief 音频文件记录结构体（项目要求全字段）
 *
 * 成员说明：
 * - id: 数据库主键ID（自增）
 * - song_name: 音频歌曲名称
 * - file_path: 音频文件本地完整路径
 * - file_size: 音频文件大小（单位：字节）
 * - cover_path: 音频封面图片文件路径
 * - composer: 音频作曲家/创作者信息
 * - audio_format: 音频编码格式（MP3、FLAC、AAC、PCM等）
 * - duration: 音频总时长（单位：毫秒）
 * - bit_depth: 音频采样位深（16bit、24bit、32bit）
 * - sample_rate: 音频采样频率（44100Hz、48000Hz、96000Hz）
 * - bit_rate: 音频比特率/码率（单位：kbps）
 * - channel_count: 音频声道数（1=单声道、2=立体声、6=5.1声道、8=7.1声道）
 * - channel_layout: 音频声道布局（stereo、5.1、7.1等）
 * - lyrics_path: 音频歌词文件本地路径
 * - create_time: 记录创建时间戳（单位：秒）
 */
struct AudioFileRecord {
    int64_t id = 0;
    std::string song_name;
    std::string file_path;
    uint64_t file_size = 0;
    std::string cover_path;
    std::string composer;
    std::string audio_format;
    uint64_t duration = 0;
    int bit_depth = 0;
    int sample_rate = 0;
    int bit_rate = 0;
    int channel_count = 0;
    std::string channel_layout;
    std::string lyrics_path;
    uint64_t create_time = 0;
};

/**
 * @class AudioDatabase
 * @brief 音频数据库抽象接口
 *
 * 负责音频文件元数据的持久化存储，客户端采用SQLite实现
 * 提供音频记录的新增、查询、更新、删除核心能力
 */
class AudioDatabase {
public:
    virtual ~AudioDatabase() = default;

    /**
     * @brief 初始化音频数据库，创建音频记录表
     * @param db_path SQLite数据库文件本地路径
     * @return 初始化成功返回true，失败返回false
     */
    virtual bool Init(const std::string& db_path) = 0;

    /**
     * @brief 新增音频文件记录到数据库
     * @param record 待新增的音频文件记录结构体
     * @return 新增成功返回记录主键ID，失败返回-1
     */
    virtual int64_t AddAudioRecord(const AudioFileRecord& record) = 0;

    /**
     * @brief 根据记录ID查询音频文件记录
     * @param id 音频记录主键ID
     * @return 查询到的音频记录，不存在则返回空结构体
     */
    virtual AudioFileRecord GetAudioRecordById(int64_t id) = 0;

    /**
     * @brief 查询数据库中所有音频文件记录
     * @return 所有音频记录的数组列表
     */
    virtual std::vector<AudioFileRecord> GetAllAudioRecords() = 0;

    /**
     * @brief 更新数据库中的音频文件记录
     * @param record 待更新的音频记录（需包含有效主键ID）
     * @return 更新成功返回true，失败返回false
     */
    virtual bool UpdateAudioRecord(const AudioFileRecord& record) = 0;

    /**
     * @brief 根据记录ID删除音频文件记录
     * @param id 音频记录主键ID
     * @return 删除成功返回true，失败返回false
     */
    virtual bool DeleteAudioRecord(int64_t id) = 0;
};

} // namespace Prism::Persistence