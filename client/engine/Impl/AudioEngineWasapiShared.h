#pragma once

#include "AudioEngine.h"

namespace Prism::Engine {

class AudioEngineWasapiShared : public AudioEngine {
public:
    AudioEngineWasapiShared();
    ~AudioEngineWasapiShared();

private:
};

} // namespace Prism::Engine
