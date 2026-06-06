#include "CommandDispatcher.h"

#include "AudioEngineFactory.h"
#include "VideoEngineFactory.h"

#include <spdlog/spdlog.h>

namespace Prism::Business {

CommandDispatcher::CommandDispatcher() = default;

bool CommandDispatcher::dispatch_play()
{
    bool ok = true;
    if (audio_engine_) {
        if (!audio_engine_->play()) {
            spdlog::warn("[CommandDispatcher] audio play failed");
            ok = false;
        }
    }
    if (video_engine_) {
        if (!video_engine_->play()) {
            spdlog::warn("[CommandDispatcher] video play failed");
            ok = false;
        }
    }
    spdlog::debug("[CommandDispatcher] dispatch_play: {}", ok);
    return ok;
}

bool CommandDispatcher::dispatch_pause()
{
    bool ok = true;
    if (audio_engine_) {
        if (!audio_engine_->pause()) {
            spdlog::warn("[CommandDispatcher] audio pause failed");
            ok = false;
        }
    }
    if (video_engine_) {
        if (!video_engine_->pause()) {
            spdlog::warn("[CommandDispatcher] video pause failed");
            ok = false;
        }
    }
    spdlog::debug("[CommandDispatcher] dispatch_pause: {}", ok);
    return ok;
}

bool CommandDispatcher::dispatch_seek(uint64_t pts, int seek_mode)
{
    bool ok = true;
    if (audio_engine_) {
        if (!audio_engine_->seek(pts, seek_mode)) {
            spdlog::warn("[CommandDispatcher] audio seek to {} failed", pts);
            ok = false;
        }
    }
    if (video_engine_) {
        if (!video_engine_->seek(pts, seek_mode)) {
            spdlog::warn("[CommandDispatcher] video seek to {} failed", pts);
            ok = false;
        }
    }
    spdlog::debug("[CommandDispatcher] dispatch_seek: pts={} mode={} ok={}", pts, seek_mode, ok);
    return ok;
}

bool CommandDispatcher::dispatch_speed(float speed)
{
    bool ok = true;
    if (audio_engine_) {
        if (!audio_engine_->set_play_speed()) {
            spdlog::warn("[CommandDispatcher] audio speed change failed");
            ok = false;
        }
    }
    if (video_engine_) {
        if (!video_engine_->set_play_speed(speed)) {
            spdlog::warn("[CommandDispatcher] video speed change failed");
            ok = false;
        }
    }
    spdlog::debug("[CommandDispatcher] dispatch_speed: {:.2f}x ok={}", speed, ok);
    return ok;
}

void CommandDispatcher::set_audio_engine(Prism::Engine::AudioEngine* engine)
{
    audio_engine_ = engine;
}

void CommandDispatcher::set_video_engine(Prism::Engine::VideoEngine* engine)
{
    video_engine_ = engine;
}

bool CommandDispatcher::initialize_engines(void* audio_factory, void* video_factory, bool enable_video)
{
    auto* audio_fac = static_cast<Prism::Engine::AudioEngineFactory*>(audio_factory);
    if (audio_fac) {
        audio_owned_ = audio_fac->create_audio_engine();
        audio_engine_ = audio_owned_.get();
        if (!audio_engine_ || !audio_engine_->init()) {
            spdlog::error("[CommandDispatcher] failed to init audio engine");
            audio_owned_.reset();
            audio_engine_ = nullptr;
            return false;
        }
        spdlog::info("[CommandDispatcher] audio engine initialized");
    }

    auto* video_fac = static_cast<Prism::Engine::VideoEngineFactory*>(video_factory);
    if (video_fac && enable_video) {
        video_owned_ = video_fac->create_video_engine();
        video_engine_ = video_owned_.get();
        if (!video_engine_ || !video_engine_->init()) {
            spdlog::error("[CommandDispatcher] failed to init video engine");
            video_owned_.reset();
            video_engine_ = nullptr;
            return false;
        }
        spdlog::info("[CommandDispatcher] video engine initialized");
    }

    return true;
}

void CommandDispatcher::shutdown_engines()
{
    if (audio_engine_) {
        audio_engine_->close();
        audio_owned_.reset();
        audio_engine_ = nullptr;
        spdlog::info("[CommandDispatcher] audio engine shutdown");
    }

    if (video_engine_) {
        video_engine_->close();
        video_owned_.reset();
        video_engine_ = nullptr;
        spdlog::info("[CommandDispatcher] video engine shutdown");
    }
}

} // namespace Prism::Business
