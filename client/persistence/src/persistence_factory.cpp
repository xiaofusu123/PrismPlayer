#include "../include/persistence_factory.h"
#include "../impl/audio_file_manager_impl.h"
#include "../impl/audio_database_impl.h"

namespace Prism::persistence {

/**
 * @brief 创建音频文件管理器实例
 * @return std::unique_ptr<AudioFileManager> 文件管理器实例智能指针
 */
std::unique_ptr<AudioFileManager> create_audio_file_manager() {
    return std::make_unique<AudioFileManagerImpl>();
}

/**
 * @brief 创建音频数据库实例
 * @return std::unique_ptr<AudioDatabase> 数据库实例智能指针
 */
std::unique_ptr<AudioDatabase> create_audio_database() {
    return std::make_unique<AudioDatabaseImpl>();
}

}