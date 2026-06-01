#pragma once
#include "../include/FileManager.h"

namespace Prism::Persistence {

/**
 * @class FileManagerStd
 * @brief 文件操作标准实现类
 *
 * 基于C++标准库文件流实现文件读写、删除、查询功能
 * 兼容Windows、Linux跨平台文件操作
 */
class FileManagerStd final : public FileManager {
public:
    /**
     * @brief 保存二进制数据到文件（标准实现）
     */
    bool SaveFile(const void* data, uint64_t size, const std::string& save_path) override;

    /**
     * @brief 从文件加载二进制数据（标准实现）
     */
    bool LoadFile(const std::string& file_path, void** out_data, uint64_t& out_size) override;

    /**
     * @brief 删除文件（标准实现）
     */
    bool DeleteFile(const std::string& file_path) override;

    /**
     * @brief 获取文件大小（标准实现）
     */
    uint64_t GetFileSize(const std::string& file_path) override;

    /**
     * @brief 检查文件是否存在（标准实现）
     */
    bool IsFileExists(const std::string& file_path) override;
};

} // namespace Prism::Persistence