#include "VideoEngineVulkan.h"

namespace Prism::Engine {

VideoEngineVulkan::VideoEngineVulkan() {

}

VideoEngineVulkan::~VideoEngineVulkan() {
    
}

bool VideoEngineVulkan::init() {
    return true;
}

bool VideoEngineVulkan::play() {
    return true;
}

bool VideoEngineVulkan::pause() {
    return true;
}

bool VideoEngineVulkan::close() {
    return true;
}

bool VideoEngineVulkan::set_play_speed(float speed) {
    return true;
}

bool VideoEngineVulkan::seek(uint64_t pts, int seek_mode) {
    return true;
}

VideoSyncInfo VideoEngineVulkan::get_sync_info() {
    return VideoSyncInfo{};
}

RenderMetadata VideoEngineVulkan::get_render_result() {
    return RenderMetadata{};
}

} // namespace Prism::Engine
