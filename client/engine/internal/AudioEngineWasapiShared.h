#pragma once

#include "AudioEngine.h"

namespace Prism::Engine {

class AudioEngineWasapiShared : public AudioEngine {
public:
    AudioEngineWasapiShared();
    ~AudioEngineWasapiShared() override;

    bool init() override;
    bool play() override;
    bool pause() override;
    bool close() override;
    bool set_play_speed() override;
    bool seek(uint64_t pts, int seek_mode) override;
    AudioSyncInfo get_sync_info() override;
    void set_observer(EngineObserver* observer) override;

private:
    EngineObserver* observer_{nullptr};
};

} // namespace Prism::Engine
