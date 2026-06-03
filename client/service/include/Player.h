#pragma once

#include "../../common/API.h"
#include "Types.h"

#include <cstdint>
#include <memory>

namespace Prism::Service {

/**
 * @class PrismPlayer
 * @brief 播放器统一 API 门面
 *
 * 组合 Business 层（AV Sync + Network）功能，为上层提供完整的
 * 播放控制接口。通过 PIMPL 模式隐藏内部实现细节。
 */
class _API PrismPlayer {
public:
    class Impl;

    /**
     * @brief 构造播放器实例
     * @param config 播放器配置（可使用默认值）
     */
    explicit PrismPlayer(const PrismConfig& config = PrismConfig{});

    /**
     * @brief 销毁播放器实例，释放所有资源
     */
    ~PrismPlayer();

    /** 禁止拷贝 */
    PrismPlayer(const PrismPlayer&) = delete;
    PrismPlayer& operator=(const PrismPlayer&) = delete;

    /* ========== 事件回调 ========== */

    /**
     * @brief 设置事件回调函数
     * @param callback 事件回调函数（可为 NULL 取消回调）
     * @param user_data 回调透传的用户数据
     */
    void set_event_callback(PrismEventCallback callback, void* user_data);

    /* ========== 媒体源 ========== */

    /**
     * @brief 打开媒体源（本地文件路径或网络 URL）
     *
     * 调用后状态变为 PRISM_STATE_LOADING，
     * 加载完成后触发 PRISM_EVENT_MEDIA_LOADED 回调。
     *
     * @param uri 文件路径或网络 URL
     * @return PRISM_OK 成功开始加载，否则返回错误码
     */
    int open(const char* uri);

    /**
     * @brief 关闭当前媒体，释放解码资源，状态回到 PRISM_STATE_IDLE
     * @return PRISM_OK
     */
    int close();

    /* ========== 播放控制 ========== */

    /**
     * @brief 开始/恢复播放
     *
     * 状态变为 PRISM_STATE_PLAYING。
     * 播放完毕后触发 PRISM_EVENT_PLAYBACK_COMPLETED，状态回到 STOPPED。
     *
     * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA
     */
    int play();

    /**
     * @brief 暂停播放，保留解码资源，状态变为 PRISM_STATE_PAUSED
     * @return PRISM_OK
     */
    int pause();

    /**
     * @brief 停止播放并释放解码资源，状态变为 PRISM_STATE_STOPPED
     *
     * 与 close 的区别：stop 保留媒体信息，可再次 play 从头播放。
     *
     * @return PRISM_OK
     */
    int stop();

    /**
     * @brief 跳转到指定位置
     *
     * 完成后触发 PRISM_EVENT_SEEK_COMPLETED。
     *
     * @param position_ms 目标位置（毫秒）
     * @param mode PRISM_SEEK_ABSOLUTE（绝对）或 PRISM_SEEK_RELATIVE（相对偏移）
     * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA，
     *         不可 seek 返回 PRISM_ERROR_SEEK_FAILED
     */
    int seek(int64_t position_ms, PrismSeekMode mode);

    /* ========== 状态查询 ========== */

    /**
     * @brief 获取当前播放状态
     * @return 当前 PrismState
     */
    PrismState get_state() const;

    /**
     * @brief 获取当前播放位置
     * @return 当前位置（毫秒），无媒体返回 -1
     */
    int64_t get_position() const;

    /**
     * @brief 获取媒体总时长
     * @return 总时长（毫秒），直播流/无媒体返回 -1
     */
    int64_t get_duration() const;

    /* ========== 音量控制 ========== */

    /**
     * @brief 设置音量
     * @param volume 音量 0.0-1.0，超出范围自动钳位
     * @return PRISM_OK
     */
    int set_volume(float volume);

    /**
     * @brief 获取当前音量
     * @return 音量值 0.0-1.0
     */
    float get_volume() const;

    /**
     * @brief 设置静音状态
     * @param mute true=静音 false=取消静音
     * @return PRISM_OK
     */
    int set_mute(bool mute);

    /**
     * @brief 获取静音状态
     * @return 是否静音
     */
    bool get_mute() const;

    /* ========== 播放属性 ========== */

    /**
     * @brief 设置播放速度
     * @param speed 速度倍率，范围 0.5x-2.0x，1.0 为正常速度，
     *              超出范围自动钳位
     * @return PRISM_OK
     */
    int set_playback_speed(float speed);

    /**
     * @brief 获取播放速度
     * @return 当前速度倍率
     */
    float get_playback_speed() const;

    /**
     * @brief 设置循环播放
     * @param loop true=单曲循环 false=播完停止
     * @return PRISM_OK
     */
    int set_loop(bool loop);

    /**
     * @brief 获取循环状态
     * @return 是否循环播放
     */
    bool get_loop() const;

    /* ========== 视频窗口 ========== */

    /**
     * @brief 设置视频渲染目标窗口
     * @param native_window 原生窗口句柄（Windows: HWND 转换为 void*），
     *                      传入 NULL 可解除绑定
     * @return PRISM_OK
     */
    int set_video_window(void* native_window);

    /* ========== 媒体信息 ========== */

    /**
     * @brief 获取已加载媒体的详细信息
     *
     * 应在收到 PRISM_EVENT_MEDIA_LOADED 之后调用。
     *
     * @param info [out] 输出参数，调用方分配内存
     * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA
     */
    int get_media_info(PrismMediaInfo* info) const;

    /* ========== 诊断工具 ========== */

    /**
     * @brief 获取最后一次失败操作的详细错误码
     * @return PrismErrorCode
     */
    PrismErrorCode get_last_error() const;

    /**
     * @brief 获取 SDK 版本字符串
     * @return 版本号，格式 "PrismPlayer x.y.z"
     */
    static const char* get_version();

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace Prism::Service
