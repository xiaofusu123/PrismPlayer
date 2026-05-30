#pragma once

#include "AudioEngineFactory.h"
#include "AudioEngineWasapiShared.h"

namespace Prism::Engine {

class AudioEngineWasapiSharedFactory : public AudioEngineFactory {
public:
    virtual ~AudioEngineWasapiSharedFactory() = default;

    /**
    * @brief 创建音频引擎实例，WasApi（共享模式）
    */
    std::unique_ptr<AudioEngine> create_audio_engine() override {
        return std::make_unique<AudioEngineWasapiShared>();
    }
};

} // namespace Prism::Engine
