#pragma once
#include "FileManagerFactory.h"

namespace Prism::Persistence {

/**
 * @class FileManagerStdFactory
 * @brief 标准文件操作专属工厂类
 *
 * 继承通用工厂，负责创建标准C++实现的文件操作实例
 * 与引擎层专属工厂设计完全对齐
 */
class FileManagerStdFactory : public FileManagerFactory {
public:
    /**
     * @brief 创建标准文件操作实例
     * @return 标准文件操作实例的智能指针
     */
    std::unique_ptr<FileManager> CreateFileManager() override;
};

} // namespace Prism::Persistence