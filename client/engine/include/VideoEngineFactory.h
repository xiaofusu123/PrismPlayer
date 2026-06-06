#pragma once

#include <memory>

#include "VideoEngine.h"

namespace Prism::Engine {

class VideoEngineFactory {
public:
    virtual ~VideoEngineFactory() = default;

    /**
     * @brief 创建视频引擎实例
     */
    virtual std::unique_ptr<VideoEngine> create_video_engine() = 0;
};

} // namespace Prism::Engine
