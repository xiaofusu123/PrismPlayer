#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== 版本信息 ========== */

#define PRISM_VERSION_MAJOR 0  /**< 主版本号 */
#define PRISM_VERSION_MINOR 0  /**< 次版本号 */
#define PRISM_VERSION_PATCH 1  /**< 修订版本号 */

/* ========== 播放状态 ========== */

/**
 * @enum PrismState
 * @brief 播放器播放状态
 */
typedef enum {
    PRISM_STATE_IDLE = 0,     /**< 空闲状态，未加载媒体 */
    PRISM_STATE_LOADING,      /**< 媒体加载中 */
    PRISM_STATE_PLAYING,      /**< 播放中 */
    PRISM_STATE_PAUSED,       /**< 已暂停 */
    PRISM_STATE_STOPPED,      /**< 已停止 */
    PRISM_STATE_ERROR         /**< 错误状态 */
} PrismState;

/* ========== 事件类型 ========== */

/**
 * @enum PrismEventType
 * @brief 播放器事件类型，通过回调通知上层
 */
typedef enum {
    PRISM_EVENT_MEDIA_LOADED = 0,      /**< 媒体加载完成 */
    PRISM_EVENT_PLAYBACK_COMPLETED,    /**< 播放完成 */
    PRISM_EVENT_SEEK_COMPLETED,        /**< Seek 操作完成 */
    PRISM_EVENT_BUFFERING_START,       /**< 缓冲开始 */
    PRISM_EVENT_BUFFERING_END,         /**< 缓冲结束 */
    PRISM_EVENT_ERROR                  /**< 发生错误 */
} PrismEventType;

/**
 * @brief 播放器事件回调函数类型
 * @param type 事件类型，标识当前发生的事件
 * @param data 事件附加数据，各事件类型的携带数据不同（可为 NULL）
 * @param user_data set_event_callback 时透传的用户数据指针
 */
typedef void (*PrismEventCallback)(PrismEventType type, const void* data, void* user_data);

/* ========== 错误码 ========== */

/**
 * @enum PrismErrorCode
 * @brief 播放器操作错误码
 */
typedef enum {
    PRISM_OK                   =  0,  /**< 操作成功 */
    PRISM_ERROR_UNKNOWN        = -1,  /**< 未知错误 */
    PRISM_ERROR_INVALID_HANDLE = -2,  /**< 无效的播放器句柄 */
    PRISM_ERROR_INVALID_PARAM  = -3,  /**< 无效的参数 */
    PRISM_ERROR_NO_MEDIA       = -4,  /**< 未加载媒体 */
    PRISM_ERROR_OPEN_FAILED    = -5,  /**< 打开媒体失败 */
    PRISM_ERROR_SEEK_FAILED    = -6,  /**< Seek 操作失败 */
    PRISM_ERROR_NOT_SUPPORTED  = -7   /**< 不支持的操作 */
} PrismErrorCode;

/* ========== Seek 模式 ========== */

/**
 * @enum PrismSeekMode
 * @brief Seek 跳转模式
 */
typedef enum {
    PRISM_SEEK_ABSOLUTE = 0,  /**< 绝对跳转，position_ms 为目标位置 */
    PRISM_SEEK_RELATIVE = 1   /**< 相对跳转，position_ms 为偏移量 */
} PrismSeekMode;

/* ========== 播放配置 ========== */

/**
 * @struct PrismConfig
 * @brief 播放器初始化配置
 */
typedef struct PrismConfig {
    int video_output_width{0};      /**< 视频输出宽度（像素），0 表示使用原始分辨率 */
    int video_output_height{0};     /**< 视频输出高度（像素），0 表示使用原始分辨率 */
    int audio_sample_rate{0};       /**< 音频采样率（Hz），0 表示使用原始采样率 */
    float default_volume{1.0f};     /**< 默认音量，范围 0.0-1.0 */
    bool enable_video{true};        /**< 是否启用视频渲染 */
    bool enable_audio{true};        /**< 是否启用音频输出 */
    const char* log_level{nullptr}; /**< spdlog 日志等级（"trace"/"debug"/"info"/"warn"/"error"），NULL 默认 "info" */
} PrismConfig;

/* ========== 媒体信息 ========== */

/**
 * @struct PrismMediaInfo
 * @brief 已加载媒体的详细信息
 */
typedef struct PrismMediaInfo {
    int video_width{0};         /**< 视频宽度（像素），无视频流时为 0 */
    int video_height{0};        /**< 视频高度（像素），无视频流时为 0 */
    int64_t duration_ms{0};     /**< 媒体总时长（毫秒），直播流为 -1 */
    int audio_channels{0};      /**< 音频声道数，无音频流时为 0 */
    int audio_sample_rate{0};   /**< 音频采样率（Hz），无音频流时为 0 */
} PrismMediaInfo;
