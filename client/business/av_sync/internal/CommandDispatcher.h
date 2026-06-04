#pragma once

#include "ICommandDispatcher.h"

#include <memory>

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
    bool initialize_engines(void* audio_factory, void* video_factory, bool enable_video) override;
    void shutdown_engines() override;

private:
    Prism::Engine::AudioEngine* audio_engine_{nullptr};
    Prism::Engine::VideoEngine* video_engine_{nullptr};
    std::unique_ptr<Prism::Engine::AudioEngine> audio_owned_;
    std::unique_ptr<Prism::Engine::VideoEngine> video_owned_;
};

} // namespace Prism::Business
