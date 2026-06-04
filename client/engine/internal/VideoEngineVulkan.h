#pragma once

#include "VideoEngine.h"

namespace Prism::Engine {

class VideoEngineVulkan : public VideoEngine {
public:
    VideoEngineVulkan();
    ~VideoEngineVulkan() override;

    virtual bool init() override;
    virtual bool play() override;
    virtual bool pause() override;
    virtual bool close() override;
    virtual bool set_play_speed(float speed) override;
    virtual bool seek(uint64_t pts, int seek_mode) override;
    virtual VideoSyncInfo get_sync_info() override;
    virtual RenderMetadata get_render_result() override;
private:
};

} // namespace Prism::Engine
