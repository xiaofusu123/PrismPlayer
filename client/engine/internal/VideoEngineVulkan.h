#pragma once

#include "VideoEngine.h"

namespace Prism::Engine {

class VideoEngineVulkan : public VideoEngine {
public:
    VideoEngineVulkan();
    ~VideoEngineVulkan() override;

    bool init() override;
    bool play() override;
    bool pause() override;
    bool close() override;
    bool set_play_speed(float speed) override;
    bool seek(uint64_t pts, int seek_mode) override;
    VideoSyncInfo get_sync_info() override;
    RenderMetadata get_render_result() override;
private:
};

} // namespace Prism::Engine
