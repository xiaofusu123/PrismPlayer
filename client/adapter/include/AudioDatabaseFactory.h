#pragma once
#include <memory>
#include "AudioDatabase.h"

namespace Prism::Persistence {

/**
 * @class AudioDatabaseFactory
 * @brief 音频数据库通用工厂类
 *
 * 遵循引擎层工厂设计模式，负责创建音频数据库实例
 * 上层模块通过该工厂获取数据库实例，解耦实现细节
 */
class AudioDatabaseFactory {
public:
    virtual ~AudioDatabaseFactory() = default;

    /**
     * @brief 创建音频数据库实例
     * @return 音频数据库实例的智能指针
     */
    virtual std::unique_ptr<AudioDatabase> CreateAudioDatabase() = 0;
};

} // namespace Prism::Persistence