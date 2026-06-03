#pragma once

#include "../include/ICommandDispatcher.h"

namespace Prism::Business {

/**
 * @class CommandDispatcher
 * @brief 指令分发实现，持有 Engine 指针并转发播放指令
 */
class CommandDispatcher : public ICommandDispatcher {
public:
    CommandDispatcher();
    ~CommandDispatcher() override = default;

    bool dispatch_play() override;
    bool dispatch_pause() override;
    bool dispatch_seek(uint64_t pts, int seek_mode) override;
    bool dispatch_speed(float speed) override;
    void set_audio_engine(Prism::Engine::AudioEngine* engine) override;
    void set_video_engine(Prism::Engine::VideoEngine* engine) override;

private:
    Prism::Engine::AudioEngine* audio_engine_{nullptr};
    Prism::Engine::VideoEngine* video_engine_{nullptr};
};

} // namespace Prism::Business
