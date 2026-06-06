#pragma once

#include <cstdint>
#include <string>

namespace Prism::Adapter {

/**
 * @struct FileMetadata
 * @brief 媒体文件元数据，音频/视频字段共存，非适用字段为零值
 */
struct FileMetadata {
    std::string file_path;               /**< 文件路径 */
    uint64_t file_size{0};               /**< 文件大小（字节） */
    std::string format;                  /**< 容器格式（MP4/MKV/MP3/FLAC 等） */
    uint64_t duration{0};                /**< 总时长（ms） */
    uint64_t bit_rate{0};                /**< 总比特率（bps） */

    /** 音频专属字段 */
    uint32_t sample_rate{0};             /**< 采样率（Hz） */
    uint32_t channels{0};                /**< 声道数（1/2/6/8） */
    uint32_t bit_depth{0};               /**< 位深（16/24/32） */
    std::string channel_layout;          /**< 声道布局（mono/stereo/5.1/7.1） */

    /** 视频专属字段 */
    uint32_t resolution_width{0};        /**< 分辨率宽度 */
    uint32_t resolution_height{0};       /**< 分辨率高度 */
    uint32_t color_depth{0};             /**< 色深（8/10 bit） */
    double frame_rate{0.0};              /**< 帧率（fps） */
    std::string codec_name;              /**< 编码器名称（H.264/H.265/AV1 等） */
};

/**
 * @class FileAdapter
 * @brief 文件 I/O 与媒体元数据提取的抽象接口
 */
class FileAdapter {
public:
    virtual ~FileAdapter() = default;

    /**
     * @brief 打开文件
     * @param file_path 文件路径
     * @return 成功返回 true
     */
    virtual bool open(const std::string& file_path) = 0;

    /**
     * @brief 关闭文件
     */
    virtual void close() = 0;

    /**
     * @brief 从文件读取数据
     * @param buffer 目标缓冲区
     * @param size 读取字节数
     * @return 实际读取的字节数
     */
    virtual size_t read(uint8_t* buffer, size_t size) = 0;

    /**
     * @brief 向文件写入数据
     * @param buffer 源缓冲区
     * @param size 写入字节数
     * @return 实际写入的字节数
     */
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;

    /**
     * @brief 移动文件读写指针
     * @param offset 偏移量
     * @param whence 基准位置（SEEK_SET/SEEK_CUR/SEEK_END）
     * @return 成功返回 true
     */
    virtual bool seek(int64_t offset, int whence) = 0;

    /**
     * @brief 获取文件大小
     * @return 文件大小（字节）
     */
    virtual uint64_t get_size() = 0;

    /**
     * @brief 获取媒体文件元数据
     * @return FileMetadata 元数据结构体
     */
    virtual FileMetadata get_metadata() = 0;

    /**
     * @brief 判断文件是否已打开
     * @return 文件打开返回 true
     */
    virtual bool is_open() = 0;
};

} // namespace Prism::Adapter
