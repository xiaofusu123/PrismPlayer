#pragma once

#include <atomic>
#include <chrono>

#ifdef _WIN32
    #include <windows.h>
typedef HANDLE RenderResultHandle;
#else
typedef int32_t RenderResultHandle;
#endif

namespace Prism::Engine {

/**
 * @struct VideoSyncInfo
 * @brief 视频同步信息，由视频引擎写、AV Sync 层读，跨线程交换
 *
 * 成员都是 atomic：只在同一进程内的线程之间使用，不序列化、不发给其它进程，
 * 也不构成任何 ABI 稳定性承诺。因为含 atomic，本结构不可拷贝、不可移动。
 *
 * 单位统一为 std::chrono::milliseconds：
 * - current_pts / next_pts：相对流起点的播放位置，绝对时间轴上的毫秒数；
 * - start_time：流开始时刻相对文件起点的偏移；
 * - duration：视频总时长（旧字段名 draution 是笔误，已按 #128 改正）。
 */
struct VideoSyncInfo {
    std::atomic<std::chrono::milliseconds> current_pts{}; /**< 当前视频帧的解码时间戳 */
    std::atomic<std::chrono::milliseconds> next_pts{};    /**< 下一帧视频帧的解码时间戳 */
    std::atomic<std::chrono::milliseconds> start_time{};  /**< 流开始时刻相对文件起点的偏移 */
    std::atomic<std::chrono::milliseconds> duration{};    /**< 视频总时长 */
};

/**
 * @struct RenderMetadata
 * @brief 渲染元数据
 */
struct RenderMetadata {
    RenderResultHandle handle; /**< 渲染结果句柄 */
    bool valid;                /**< 句柄是否有效 */

    uint32_t width;     /**< 渲染宽度 */
    uint32_t height;    /**< 渲染高度 */
    uint32_t format;    /**< 像素格式 */
    uint64_t timestamp; /**< 时间戳 */
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
     * @param speed 播放速率，1.0 为原速，>1 加速、<1 减速；必须为正数
     */
    virtual bool set_play_speed(float speed) = 0;

    /**
     * @brief 视频跳转
     * @param pts 跳转目标位置（绝对位置，相对流起点）
     *
     * 只接受绝对位置：相对快进/倒退由上层（Service / AV Sync）换算成绝对位置后再调用。
     */
    virtual bool seek(std::chrono::milliseconds pts) = 0;

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
