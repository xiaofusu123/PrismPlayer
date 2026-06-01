#pragma once
#include "AudioDatabaseFactory.h"

namespace Prism::Persistence {

/**
 * @class AudioDatabaseSQLiteFactory
 * @brief SQLite音频数据库专属工厂类
 *
 * 继承通用工厂，负责创建SQLite实现的音频数据库实例
 * 与引擎层专属工厂设计完全对齐
 */
class AudioDatabaseSQLiteFactory : public AudioDatabaseFactory {
public:
    /**
     * @brief 创建SQLite音频数据库实例
     * @return SQLite音频数据库实例的智能指针
     */
    std::unique_ptr<AudioDatabase> CreateAudioDatabase() override;
};

} // namespace Prism::Persistence