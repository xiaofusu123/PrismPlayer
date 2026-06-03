#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Prism::Adapter {

/**
 * @struct AccountInfo
 * @brief 用户账户信息
 */
struct AccountInfo {
    int64_t id{0};                       /**< 账户 ID */
    std::string username;                /**< 账户名 / 昵称 */
    std::string email;                   /**< 邮箱 */
    std::string password_hash;           /**< 密码哈希 */
};

/**
 * @struct AudioFileInfo
 * @brief 音频文件信息
 */
struct AudioFileInfo {
    int64_t id{0};                       /**< 文件 ID */
    std::string title;                   /**< 歌曲名 */
    std::string file_path;               /**< 文件路径 */
    uint64_t file_size{0};               /**< 文件大小（字节） */
    std::string cover_path;              /**< 封面路径 */
    std::string artist;                  /**< 作曲者 */
    std::string encoding_format;         /**< 编码格式（AAC/MP3/FLAC/Opus/PCM） */
    uint64_t duration{0};                /**< 时长（ms） */
    uint32_t bit_depth{0};               /**< 位深（16/24/32） */
    uint32_t sample_rate{0};             /**< 采样率（Hz） */
    uint64_t bit_rate{0};                /**< 比特率（bps） */
    uint32_t channels{0};                /**< 声道数（1/2/6/8） */
    std::string channel_layout;          /**< 声道布局（mono/stereo/5.1/7.1） */
    std::string lyrics_path;             /**< 歌词文件路径 */
};

/**
 * @struct VideoFileInfo
 * @brief 视频文件信息
 */
struct VideoFileInfo {
    int64_t id{0};                       /**< 文件 ID */
    std::string filename;                /**< 文件名 */
    std::string file_path;               /**< 文件路径 */
    uint64_t file_size{0};               /**< 文件大小（字节） */
    std::string author;                  /**< 作者 */
    uint32_t resolution_width{0};        /**< 分辨率宽度 */
    uint32_t resolution_height{0};       /**< 分辨率高度 */
    uint32_t color_depth{0};             /**< 色深（8/10 bit） */
    uint64_t duration{0};                /**< 时长（ms） */
    std::string encoding_format;         /**< 编码格式（H.264/H.265/AV1/VP9） */
    double frame_rate{0.0};              /**< 帧率（fps） */
    uint64_t bit_rate{0};                /**< 比特率（bps） */
};

/**
 * @class DBAdapter
 * @brief SQLite 数据库操作抽象接口，提供表级 CRUD
 */
class DBAdapter {
public:
    virtual ~DBAdapter() = default;

    /**
     * @brief 初始化数据库连接
     * @param db_path 数据库文件路径
     * @return 成功返回 true
     */
    virtual bool init(const std::string& db_path) = 0;

    /**
     * @brief 关闭数据库连接
     */
    virtual void close() = 0;

    // ==================== Account 账户操作 ====================

    /**
     * @brief 插入账户
     * @param info 账户信息
     * @return 成功返回 true
     */
    virtual bool insert_account(const AccountInfo& info) = 0;

    /**
     * @brief 按用户名查询账户
     * @param username 用户名
     * @return 找到返回 AccountInfo，否则返回 nullopt
     */
    virtual std::optional<AccountInfo> query_account_by_username(const std::string& username) = 0;

    /**
     * @brief 更新账户信息
     * @param info 账户信息
     * @return 成功返回 true
     */
    virtual bool update_account(const AccountInfo& info) = 0;

    /**
     * @brief 删除账户
     * @param id 账户 ID
     * @return 成功返回 true
     */
    virtual bool delete_account(int64_t id) = 0;

    // ==================== Audio 音频文件操作 ====================

    /**
     * @brief 插入音频文件记录
     * @param info 音频文件信息
     * @return 成功返回 true
     */
    virtual bool insert_audio_file(const AudioFileInfo& info) = 0;

    /**
     * @brief 查询全部音频文件
     * @return 音频文件列表
     */
    virtual std::vector<AudioFileInfo> query_audio_files() = 0;

    /**
     * @brief 按 ID 查询音频文件
     * @param id 文件 ID
     * @return 找到返回 AudioFileInfo，否则返回 nullopt
     */
    virtual std::optional<AudioFileInfo> query_audio_file_by_id(int64_t id) = 0;

    /**
     * @brief 更新音频文件记录
     * @param info 音频文件信息
     * @return 成功返回 true
     */
    virtual bool update_audio_file(const AudioFileInfo& info) = 0;

    /**
     * @brief 删除音频文件记录
     * @param id 文件 ID
     * @return 成功返回 true
     */
    virtual bool delete_audio_file(int64_t id) = 0;

    // ==================== Video 视频文件操作 ====================

    /**
     * @brief 插入视频文件记录
     * @param info 视频文件信息
     * @return 成功返回 true
     */
    virtual bool insert_video_file(const VideoFileInfo& info) = 0;

    /**
     * @brief 查询全部视频文件
     * @return 视频文件列表
     */
    virtual std::vector<VideoFileInfo> query_video_files() = 0;

    /**
     * @brief 按 ID 查询视频文件
     * @param id 文件 ID
     * @return 找到返回 VideoFileInfo，否则返回 nullopt
     */
    virtual std::optional<VideoFileInfo> query_video_file_by_id(int64_t id) = 0;

    /**
     * @brief 更新视频文件记录
     * @param info 视频文件信息
     * @return 成功返回 true
     */
    virtual bool update_video_file(const VideoFileInfo& info) = 0;

    /**
     * @brief 删除视频文件记录
     * @param id 文件 ID
     * @return 成功返回 true
     */
    virtual bool delete_video_file(int64_t id) = 0;

    // ==================== Settings 设置操作 ====================

    /**
     * @brief 获取设置项
     * @param key 设置键名
     * @return 找到返回值，否则返回 nullopt
     */
    virtual std::optional<std::string> get_setting(const std::string& key) = 0;

    /**
     * @brief 设置（新增或更新）设置项
     * @param key 设置键名
     * @param value 设置值
     * @return 成功返回 true
     */
    virtual bool set_setting(const std::string& key, const std::string& value) = 0;
};

} // namespace Prism::Adapter
