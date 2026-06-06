#include "AudioEngineWasapiSharedFactory.h"

#include "AudioEngineWasapiShared.h"

namespace Prism::Engine {

std::unique_ptr<AudioEngine> AudioEngineWasapiSharedFactory::create_audio_engine() {
    return std::make_unique<AudioEngineWasapiShared>();
}

} // namespace Prism::Engine
