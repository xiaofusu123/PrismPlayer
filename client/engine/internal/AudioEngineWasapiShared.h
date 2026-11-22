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
    bool set_play_speed(float speed) override;
    bool seek(std::chrono::milliseconds pts) override;
    AudioSyncInfo get_sync_info() override;

  private:
};

} // namespace Prism::Engine
