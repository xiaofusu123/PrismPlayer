#include "../include/AudioDatabaseSQLiteFactory.h"
#include "../Impl/AudioDatabaseSQLite.h"

namespace Prism::Persistence {

/**
 * @brief 创建SQLite音频数据库实例
 * @return SQLite音频数据库实例的智能指针
 */
std::unique_ptr<AudioDatabase> AudioDatabaseSQLiteFactory::CreateAudioDatabase() {
    return std::make_unique<AudioDatabaseSQLite>();
}

} // namespace Prism::Persistence