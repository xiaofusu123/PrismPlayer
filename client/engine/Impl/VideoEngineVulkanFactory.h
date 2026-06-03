#pragma once

#include "VideoEngineFactory.h"
#include "VideoEngineVulkan.h"

namespace Prism::Engine {

class VideoEngineVulkanFactory : public VideoEngineFactory {
public:
    ~VideoEngineVulkanFactory() override = default;

    /**
    * @brief 创建Vulkan渲染视频引擎实例
    */
    std::unique_ptr<VideoEngine> create_audio_engine() override {
        return std::make_unique<VideoEngineVulkan>();
    }
};

} // namespace Prism::Engine
