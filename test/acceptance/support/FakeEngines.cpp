#include "FakeEngines.h"

#include <utility>

namespace Prism::Qa {

namespace {

void record_seek(EngineRecorder& recorder, std::chrono::milliseconds pts) {
    recorder.seek_calls.fetch_add(1);
    recorder.last_seek_ms.store(pts.count());
}

void record_speed(EngineRecorder& recorder, float speed) {
    recorder.speed_calls.fetch_add(1);
    recorder.last_speed.store(speed);
}

} // namespace

/* ========== FakeAudioEngine ========== */

FakeAudioEngine::FakeAudioEngine(std::shared_ptr<EngineRecorder> recorder, bool init_ok)
    : recorder_(std::move(recorder)), init_ok_(init_ok) {}

bool FakeAudioEngine::init() {
    recorder_->init_calls.fetch_add(1);
    return init_ok_;
}

bool FakeAudioEngine::play() {
    recorder_->play_calls.fetch_add(1);
    return true;
}

bool FakeAudioEngine::pause() {
    recorder_->pause_calls.fetch_add(1);
    return true;
}

bool FakeAudioEngine::close() {
    recorder_->close_calls.fetch_add(1);
    return true;
}

bool FakeAudioEngine::set_play_speed(float speed) {
    record_speed(*recorder_, speed);
    return true;
}

bool FakeAudioEngine::seek(std::chrono::milliseconds pts) {
    record_seek(*recorder_, pts);
    return true;
}

Prism::Engine::AudioSyncInfo FakeAudioEngine::get_sync_info() {
    return Prism::Engine::AudioSyncInfo{};
}

/* ========== FakeVideoEngine ========== */

FakeVideoEngine::FakeVideoEngine(std::shared_ptr<EngineRecorder> recorder, bool init_ok)
    : recorder_(std::move(recorder)), init_ok_(init_ok) {}

bool FakeVideoEngine::init() {
    recorder_->init_calls.fetch_add(1);
    return init_ok_;
}

bool FakeVideoEngine::play() {
    recorder_->play_calls.fetch_add(1);
    return true;
}

bool FakeVideoEngine::pause() {
    recorder_->pause_calls.fetch_add(1);
    return true;
}

bool FakeVideoEngine::close() {
    recorder_->close_calls.fetch_add(1);
    return true;
}

bool FakeVideoEngine::set_play_speed(float speed) {
    record_speed(*recorder_, speed);
    return true;
}

bool FakeVideoEngine::seek(std::chrono::milliseconds pts) {
    record_seek(*recorder_, pts);
    return true;
}

Prism::Engine::VideoSyncInfo FakeVideoEngine::get_sync_info() {
    return Prism::Engine::VideoSyncInfo{};
}

Prism::Engine::RenderMetadata FakeVideoEngine::get_render_result() {
    return Prism::Engine::RenderMetadata{};
}

/* ========== FakeAudioFactory ========== */

FakeAudioFactory::FakeAudioFactory() : FakeAudioFactory(std::make_shared<EngineRecorder>(), true) {}

FakeAudioFactory::FakeAudioFactory(std::shared_ptr<EngineRecorder> recorder, bool init_ok)
    : recorder_(std::move(recorder)), init_ok_(init_ok) {}

std::unique_ptr<Prism::Engine::AudioEngine> FakeAudioFactory::create_audio_engine() {
    recorder_->create_calls.fetch_add(1);
    return std::make_unique<FakeAudioEngine>(recorder_, init_ok_);
}

/* ========== FakeVideoFactory ========== */

FakeVideoFactory::FakeVideoFactory() : FakeVideoFactory(std::make_shared<EngineRecorder>(), true) {}

FakeVideoFactory::FakeVideoFactory(std::shared_ptr<EngineRecorder> recorder, bool init_ok)
    : recorder_(std::move(recorder)), init_ok_(init_ok) {}

std::unique_ptr<Prism::Engine::VideoEngine> FakeVideoFactory::create_video_engine() {
    recorder_->create_calls.fetch_add(1);
    return std::make_unique<FakeVideoEngine>(recorder_, init_ok_);
}

} // namespace Prism::Qa
