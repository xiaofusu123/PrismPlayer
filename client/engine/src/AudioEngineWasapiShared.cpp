#include "AudioEngineWasapiShared.h"

namespace Prism::Engine {

AudioEngineWasapiShared::AudioEngineWasapiShared() {

}

AudioEngineWasapiShared::~AudioEngineWasapiShared() {
    
}

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

bool AudioEngineWasapiShared::set_play_speed() {
    return true;
}

bool AudioEngineWasapiShared::seek(uint64_t pts, int seek_mode) {
    return true;
}

AudioSyncInfo AudioEngineWasapiShared::get_sync_info() {
    return AudioSyncInfo{};
}

} // namespace Prism::Engine
