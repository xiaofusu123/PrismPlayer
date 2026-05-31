#include "../impl/audio_file_manager_impl.h"
#include <fstream>
#include <string>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace Prism::persistence {

/**
 * @brief 保存二进制音频数据到本地文件
 * @param data 音频二进制数据指针
 * @param size 音频数据大小（字节）
 * @param save_path 文件保存路径
 * @return 成功返回 true，失败返回 false
 */
bool AudioFileManagerImpl::save_file(const void* data, uint64_t size, const std::string& save_path) {
    std::ofstream file(save_path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("save file failed: {}", save_path);
        return false;
    }
    file.write(static_cast<const char*>(data), size);
    return true;
}

/**
 * @brief 从本地文件加载音频二进制数据到内存
 * @param file_path 文件加载路径
 * @param out_data 输出数据指针
 * @param out_size 输出数据大小
 * @return 成功返回 true，失败返回 false
 */
bool AudioFileManagerImpl::load_file(const std::string& file_path, void** out_data, uint64_t& out_size) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        spdlog::error("load file failed: {}", file_path);
        return false;
    }
    out_size = static_cast<uint64_t>(file.tellg());
    *out_data = new char[out_size];
    file.seekg(0);
    file.read(static_cast<char*>(*out_data), out_size);
    return true;
}

/**
 * @brief 删除本地指定音频文件
 * @param file_path 文件删除路径
 * @return 成功返回 true，失败返回 false
 */
bool AudioFileManagerImpl::delete_file(const std::string& file_path) {
    std::error_code ec;
    return std::filesystem::remove(file_path, ec);
}

/**
 * @brief 获取本地音频文件的基础信息
 * @param file_path 文件查询路径
 * @return AudioFileInfo 音频文件信息结构体
 */
AudioFileInfo AudioFileManagerImpl::get_file_info(const std::string& file_path) {
    if (!std::filesystem::exists(file_path)) {
        return {};
    }
    return AudioFileInfo{
        file_path,
        std::filesystem::file_size(file_path),
        "", 0, 0, 0, 0
    };
}

}