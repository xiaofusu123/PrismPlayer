#pragma once

#include <memory>

#include "AudioEngine.h"

namespace Prism::Engine {

class AudioEngineFactory {
public:
    virtual ~AudioEngineFactory() = default;

    /**
    * @brief 创建音频引擎实例
    */
    virtual std::unique_ptr<AudioEngine> create_audio_engine() = 0;
};

} // namespace Prism::Engine
