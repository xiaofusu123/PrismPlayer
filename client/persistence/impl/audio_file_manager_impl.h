#pragma once
#include "../include/audio_file_manager.h"

namespace Prism::persistence {

/**
 * @class AudioFileManagerImpl
 * @brief 音频文件操作接口的具体实现类
 * 
 * 基于标准文件流实现音频文件的读写、删除、信息查询功能
 */
class AudioFileManagerImpl final : public AudioFileManager {
public:
    /**
     * @brief 保存音频数据到文件（实现）
     */
    bool save_file(const void* data, uint64_t size, const std::string& save_path) override;

    /**
     * @brief 从文件加载音频数据（实现）
     */
    bool load_file(const std::string& file_path, void** out_data, uint64_t& out_size) override;

    /**
     * @brief 删除音频文件（实现）
     */
    bool delete_file(const std::string& file_path) override;

    /**
     * @brief 获取音频文件信息（实现）
     */
    AudioFileInfo get_file_info(const std::string& file_path) override;
};

}