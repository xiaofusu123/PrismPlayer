#pragma once

#include <atomic>

#ifdef _WIN32
    #include <windows.h>
    typedef HANDLE RENDER_RESULT_HANDLE;
#else
    typedef int32_t RESULTHANDLE;
#endif

namespace Prism::Engine {

/**
 * @struct VideoSyncInfo
 * @brief 视频同步信息
 */
struct VideoSyncInfo {
    std::atomic<uint64_t> current_pts{0};  /**< 当前视频帧解码时间戳（ms） */
    std::atomic<uint64_t> next_pts{0};     /**< 下一帧视频解码时间戳（ms） */
    std::atomic<uint32_t> start_time{0};   /**< 流开始的时间 */
    std::atomic<uint64_t> draution{0};     /**< 视频总时长 */
};

/**
 * @struct RenderMetadata
 * @brief 渲染元数据
 */
struct RenderMetadata {
    RENDER_RESULT_HANDLE handle;  /**< 渲染结果句柄 */
    bool valid;                   /**< 句柄是否有效 */

    uint32_t width;               /**< 渲染宽度 */
    uint32_t height;              /**< 渲染高度 */
    uint32_t format;              /**< 像素格式 */
    uint64_t timestamp;           /**< 时间戳 */
};


class VideoEngine {
public:
    virtual ~VideoEngine() = default;

    /**
     * @brief 初始化视频引擎
     */
    virtual bool init() = 0;

    /**
     * @brief 播放视频
     */
    virtual bool play() = 0;

    /**
     * @brief 暂停视频
     */
    virtual bool pause() = 0;

    /**
     * @brief 关闭视频
     */
    virtual bool close() = 0;

    /**
     * @brief 设置播放速度（倍速播放）
     * @param speed 设置的速度
     */
    virtual bool set_play_speed(float speed) = 0;

    /**
     * @brief 视频跳转
     * @param pts 跳转的时间戳
     * @param seek_mode 跳转模式。0为绝对模式，直接跳转到视频对应的时间，此时pts为非负值；1为相对时间，快进或倒退对应时间
     */
    virtual bool seek(uint64_t pts, int seek_mode) = 0;


    /**
     * @brief 获取视频同步信息
     * @return VideoSyncInfo 视频同步信息
     */
    virtual VideoSyncInfo get_sync_info() = 0;

    /**
     * @brief 获取视频渲染结果
     * @return RenderMetadata 渲染元数据
     */
    virtual RenderMetadata get_render_result() = 0;
};

} // namespace Prism::Engine
