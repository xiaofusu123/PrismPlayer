#pragma once
#include <string>
#include <cstdint>

namespace Prism::Persistence {

/**
 * @class FileManager
 * @brief 文件操作抽象接口
 *
 * 负责本地文件的读写、删除、属性查询，支持音频、封面、歌词等文件
 * 客户端采用标准C++文件流实现，供上层引擎层调用
 */
class FileManager {
public:
    virtual ~FileManager() = default;

    /**
     * @brief 保存二进制数据到本地文件
     * @param data 待保存的二进制数据指针
     * @param size 二进制数据大小（单位：字节）
     * @param save_path 文件保存的本地完整路径
     * @return 保存成功返回true，失败返回false
     */
    virtual bool SaveFile(const void* data, uint64_t size, const std::string& save_path) = 0;

    /**
     * @brief 从本地文件加载二进制数据到内存
     * @param file_path 待加载文件的本地完整路径
     * @param out_data 输出参数，存储加载后的二进制数据指针
     * @param out_size 输出参数，存储加载后的二进制数据大小（单位：字节）
     * @return 加载成功返回true，失败返回false
     */
    virtual bool LoadFile(const std::string& file_path, void** out_data, uint64_t& out_size) = 0;

    /**
     * @brief 删除本地指定文件
     * @param file_path 待删除文件的本地完整路径
     * @return 删除成功返回true，失败返回false
     */
    virtual bool DeleteFile(const std::string& file_path) = 0;

    /**
     * @brief 获取本地文件大小
     * @param file_path 待查询文件的本地完整路径
     * @return 文件大小（单位：字节），文件不存在返回0
     */
    virtual uint64_t GetFileSize(const std::string& file_path) = 0;

    /**
     * @brief 检查本地文件是否存在
     * @param file_path 待检查文件的本地完整路径
     * @return 文件存在返回true，不存在返回false
     */
    virtual bool IsFileExists(const std::string& file_path) = 0;
};

} // namespace Prism::Persistence