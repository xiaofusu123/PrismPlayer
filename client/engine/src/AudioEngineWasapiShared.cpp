#include "AudioEngineWasapiShared.h"

namespace Prism::Engine {

AudioEngineWasapiShared::AudioEngineWasapiShared() = default;

AudioEngineWasapiShared::~AudioEngineWasapiShared() = default;

bool AudioEngineWasapiShared::init() {
    return true;
}

bool AudioEngineWasapiShared::play() {
    return true;
}

bool AudioEngineWasapiShared::pause() {
    return true;
}

bool AudioEngineWasapiShared::close() {
    return true;
}

bool AudioEngineWasapiShared::set_play_speed(float speed) {
    return true;
}

bool AudioEngineWasapiShared::seek(std::chrono::milliseconds pts) {
    return true;
}

AudioSyncInfo AudioEngineWasapiShared::get_sync_info() {
    return AudioSyncInfo{};
}

} // namespace Prism::Engine
