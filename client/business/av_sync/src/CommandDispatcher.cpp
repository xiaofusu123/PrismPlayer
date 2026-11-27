#include "CommandDispatcher.h"

#include "AudioEngineFactory.h"
#include "IEngineObserverSink.h"
#include "VideoEngineFactory.h"

#include <spdlog/spdlog.h>

namespace Prism::Business {

CommandDispatcher::CommandDispatcher() = default;

bool CommandDispatcher::dispatch_play() {
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

bool CommandDispatcher::dispatch_pause() {
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

bool CommandDispatcher::dispatch_seek(std::chrono::milliseconds pts) {
    bool ok = true;
    if (audio_engine_) {
        if (!audio_engine_->seek(pts)) {
            spdlog::warn("[CommandDispatcher] audio seek to {}ms failed", pts.count());
            ok = false;
        }
    }
    if (video_engine_) {
        if (!video_engine_->seek(pts)) {
            spdlog::warn("[CommandDispatcher] video seek to {}ms failed", pts.count());
            ok = false;
        }
    }
    spdlog::debug("[CommandDispatcher] dispatch_seek: pts={}ms ok={}", pts.count(), ok);
    return ok;
}

bool CommandDispatcher::dispatch_speed(float speed) {
    bool ok = true;
    if (audio_engine_) {
        if (!audio_engine_->set_play_speed(speed)) {
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

void CommandDispatcher::set_audio_engine(Prism::Engine::AudioEngine* engine) {
    audio_engine_ = engine;
    attach_observer();
}

void CommandDispatcher::set_video_engine(Prism::Engine::VideoEngine* engine) {
    video_engine_ = engine;
    attach_observer();
}

bool CommandDispatcher::initialize_engines(void* audio_factory, void* video_factory,
                                           bool enable_video) {
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

    attach_observer();
    return true;
}

void CommandDispatcher::set_frame_observer(IEngineObserver* observer) {
    frame_observer_ = observer;
    attach_observer();
}

MediaSnapshot CommandDispatcher::get_media_snapshot() const {
    MediaSnapshot snapshot;

    // AudioSyncInfo / VideoSyncInfo 含 atomic，不可拷贝；只读取需要的成员。
    if (audio_engine_) {
        snapshot.audio_duration_ms = audio_engine_->get_sync_info().duration.load().count();
    }
    if (video_engine_) {
        snapshot.video_duration_ms = video_engine_->get_sync_info().duration.load().count();
        const Prism::Engine::RenderMetadata RENDER_METADATA = video_engine_->get_render_result();
        snapshot.video_width = RENDER_METADATA.width;
        snapshot.video_height = RENDER_METADATA.height;
    }

    spdlog::debug("[CommandDispatcher] media snapshot: audio={}ms video={}ms {}x{}",
                  snapshot.audio_duration_ms, snapshot.video_duration_ms, snapshot.video_width,
                  snapshot.video_height);
    return snapshot;
}

void CommandDispatcher::attach_observer() {
    if (!frame_observer_) {
        return;
    }
    if (audio_engine_) {
        if (auto* sink = dynamic_cast<IEngineObserverSink*>(audio_engine_)) {
            sink->set_frame_observer(frame_observer_);
        }
    }
    if (video_engine_) {
        if (auto* sink = dynamic_cast<IEngineObserverSink*>(video_engine_)) {
            sink->set_frame_observer(frame_observer_);
        }
    }
}

void CommandDispatcher::detach_observer() {
    if (audio_engine_) {
        if (auto* sink = dynamic_cast<IEngineObserverSink*>(audio_engine_)) {
            sink->set_frame_observer(nullptr);
        }
    }
    if (video_engine_) {
        if (auto* sink = dynamic_cast<IEngineObserverSink*>(video_engine_)) {
            sink->set_frame_observer(nullptr);
        }
    }
}

void CommandDispatcher::shutdown_engines() {
    detach_observer();

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
