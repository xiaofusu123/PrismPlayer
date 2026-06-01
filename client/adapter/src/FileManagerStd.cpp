#include "../Impl/FileManagerStd.h"
#include <fstream>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace Prism::Persistence {
namespace fs = std::filesystem;

/**
 * @brief 保存二进制数据到本地文件
 * @param data 待保存的二进制数据指针
 * @param size 二进制数据大小（字节）
 * @param path 文件保存路径
 * @return 保存成功返回true，失败返回false
 */
bool FileManagerStd::SaveFile(const void* data, uint64_t size, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*)data, size);
    return true;
}

/**
 * @brief 从本地文件加载二进制数据到内存
 * @param path 文件加载路径
 * @param out_data 输出数据指针
 * @param out_size 输出数据大小（字节）
 * @return 加载成功返回true，失败返回false
 */
bool FileManagerStd::LoadFile(const std::string& path, void** out_data, uint64_t& out_size) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    out_size = f.tellg();
    *out_data = new char[out_size];
    f.seekg(0);
    f.read((char*)*out_data, out_size);
    return true;
}

/**
 * @brief 删除本地指定文件
 * @param path 文件删除路径
 * @return 删除成功返回true，失败返回false
 */
bool FileManagerStd::DeleteFile(const std::string& path) {
    std::error_code ec;
    return fs::remove(path, ec);
}

/**
 * @brief 获取本地文件大小
 * @param path 文件查询路径
 * @return 文件大小（字节），文件不存在返回0
 */
uint64_t FileManagerStd::GetFileSize(const std::string& path) {
    std::error_code ec;
    return fs::exists(path) ? fs::file_size(path, ec) : 0;
}

/**
 * @brief 检查本地文件是否存在
 * @param path 文件检查路径
 * @return 文件存在返回true，不存在返回false
 */
bool FileManagerStd::IsFileExists(const std::string& path) {
    return fs::exists(path);
}

} // namespace Prism::Persistence