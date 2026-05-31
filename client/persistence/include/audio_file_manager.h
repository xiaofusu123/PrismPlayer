#pragma once
#include <string>
#include <cstdint>

namespace Prism::persistence {

/**
 * @struct AudioFileInfo
 * @brief 音频文件基础信息结构体（数据映射层）
 * 
 * 结构体成员说明：
 * 
 * - file_path      音频文件完整路径
 * - file_size      音频文件大小（字节）
 * - format          音频格式：AAC、MP3、Opus、FLAC、PCM 等
 * - bit_rate        音频比特率（bps）
 * - sample_rate    音频采样率（Hz）
 * - channels        音频声道数：1单声道、2立体声、6为5.1声道、8为7.1声道
 * - duration        音频总时长（毫秒）
 */
struct AudioFileInfo {
    std::string file_path;
    uint64_t file_size = 0;
    std::string format;
    int bit_rate = 0;
    int sample_rate = 0;
    int channels = 0;
    uint64_t duration = 0;
};

/**
 * @class AudioFileManager
 * @brief 音频文件操作抽象接口（持久化层）
 * 
 * 提供音频文件的保存、加载、删除、信息获取能力，供上层引擎层调用
 */
class AudioFileManager {
public:
    virtual ~AudioFileManager() = default;

    /**
     * @brief 保存二进制音频数据到本地文件
     * @param data 待保存的音频二进制数据指针
     * @param size 音频数据大小（字节）
     * @param save_path 音频文件保存的完整路径
     * @return 保存成功返回 true，失败返回 false
     */
    virtual bool save_file(const void* data, uint64_t size, const std::string& save_path) = 0;

    /**
     * @brief 从本地文件加载音频二进制数据到内存
     * @param file_path 待加载的音频文件完整路径
     * @param out_data 输出参数，存储加载后的音频数据指针
     * @param out_size 输出参数，存储加载后的音频数据大小（字节）
     * @return 加载成功返回 true，失败返回 false
     */
    virtual bool load_file(const std::string& file_path, void** out_data, uint64_t& out_size) = 0;

    /**
     * @brief 删除本地指定音频文件
     * @param file_path 待删除的音频文件完整路径
     * @return 删除成功返回 true，失败返回 false
     */
    virtual bool delete_file(const std::string& file_path) = 0;

    /**
     * @brief 获取本地音频文件的基础信息
     * @param file_path 待查询的音频文件完整路径
     * @return AudioFileInfo 音频文件基础信息结构体
     */
    virtual AudioFileInfo get_file_info(const std::string& file_path) = 0;
};

}