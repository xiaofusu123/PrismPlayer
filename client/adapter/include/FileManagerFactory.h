#pragma once
#include <memory>
#include "FileManager.h"

namespace Prism::Persistence {

/**
 * @class FileManagerFactory
 * @brief 文件操作通用工厂类
 *
 * 遵循引擎层工厂设计模式，负责创建文件操作实例
 * 上层模块通过该工厂获取文件操作实例，解耦实现细节
 */
class FileManagerFactory {
public:
    virtual ~FileManagerFactory() = default;

    /**
     * @brief 创建文件操作实例
     * @return 文件操作实例的智能指针
     */
    virtual std::unique_ptr<FileManager> CreateFileManager() = 0;
};

} // namespace Prism::Persistence