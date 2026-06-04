#include "VideoEngineVulkanFactory.h"

#include "VideoEngineVulkan.h"

namespace Prism::Engine {

std::unique_ptr<VideoEngine> VideoEngineVulkanFactory::create_video_engine() {
    return std::make_unique<VideoEngineVulkan>();
}

} // namespace Prism::Engine
