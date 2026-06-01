#include "../include/FileManagerStdFactory.h"
#include "../Impl/FileManagerStd.h"

namespace Prism::Persistence {

/**
 * @brief 创建标准文件操作实例
 * @return 标准文件操作实例的智能指针
 */
std::unique_ptr<FileManager> FileManagerStdFactory::CreateFileManager() {
    return std::make_unique<FileManagerStd>();
}

} // namespace Prism::Persistence