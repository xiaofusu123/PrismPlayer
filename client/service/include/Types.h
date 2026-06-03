#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 版本信息 ========== */
#define PRISM_VERSION_MAJOR 0
#define PRISM_VERSION_MINOR 0
#define PRISM_VERSION_PATCH 1

/* ========== 不透明句柄 ========== */
typedef void* PrismPlayerHandle;

/* ========== 播放状态 ========== */
typedef enum {
    PRISM_STATE_IDLE = 0,
    PRISM_STATE_LOADING,
    PRISM_STATE_PLAYING,
    PRISM_STATE_PAUSED,
    PRISM_STATE_STOPPED,
    PRISM_STATE_ERROR
} PrismState;

/* ========== 事件类型 ========== */
typedef enum {
    PRISM_EVENT_MEDIA_LOADED = 0,      /**< 媒体加载完成 */
    PRISM_EVENT_PLAYBACK_COMPLETED,    /**< 播放完毕 */
    PRISM_EVENT_SEEK_COMPLETED,        /**< 跳转完成 */
    PRISM_EVENT_BUFFERING_START,       /**< 缓冲开始 */
    PRISM_EVENT_BUFFERING_END,         /**< 缓冲结束 */
    PRISM_EVENT_ERROR,                 /**< 通用错误 */
    PRISM_EVENT_LOGIN_SUCCESS,         /**< 登录成功 */
    PRISM_EVENT_LOGIN_FAILED,          /**< 登录失败 */
    PRISM_EVENT_ROOM_JOINED,           /**< 成功加入房间 */
    PRISM_EVENT_ROOM_LEFT,             /**< 已离开房间 */
    PRISM_EVENT_CONNECTION_LOST        /**< 与服务器连接断开 */
} PrismEventType;

typedef void (*PrismEventCallback)(PrismEventType type, const void* data, void* user_data);

/* ========== 错误码 ========== */
typedef enum {
    PRISM_OK                    =  0,
    PRISM_ERROR_UNKNOWN         = -1,
    PRISM_ERROR_INVALID_HANDLE  = -2,
    PRISM_ERROR_INVALID_PARAM   = -3,
    PRISM_ERROR_NO_MEDIA        = -4,
    PRISM_ERROR_OPEN_FAILED     = -5,
    PRISM_ERROR_SEEK_FAILED     = -6,
    PRISM_ERROR_NOT_SUPPORTED   = -7,
    PRISM_ERROR_NETWORK         = -8,
    PRISM_ERROR_AUTH_FAILED     = -9,
    PRISM_ERROR_ROOM_FULL       = -10
} PrismErrorCode;

/* ========== Seek 模式 ========== */
typedef enum {
    PRISM_SEEK_ABSOLUTE = 0,  /**< 绝对模式 */
    PRISM_SEEK_RELATIVE = 1   /**< 相对模式 */
} PrismSeekMode;

/* ========== 房间状态 ========== */
typedef enum {
    PRISM_ROOM_DISCONNECTED = 0,  /**< 未连接 */
    PRISM_ROOM_CONNECTING,        /**< 连接中 */
    PRISM_ROOM_CONNECTED,         /**< 已连接 */
    PRISM_ROOM_ERROR              /**< 连接错误 */
} PrismRoomState;

/* ========== 播放配置 ========== */
typedef struct {
    int video_output_width;    /**< 视频输出宽度，0 为自动 */
    int video_output_height;   /**< 视频输出高度，0 为自动 */
    int audio_sample_rate;     /**< 音频采样率，0 为自动 */
    float default_volume;      /**< 默认音量 0.0-1.0 */
    bool enable_video;         /**< 是否启用视频渲染 */
    bool enable_audio;         /**< 是否启用音频渲染 */
    const char* log_level;     /**< spdlog 日志级别 */
} PrismConfig;

/* ========== 媒体信息 ========== */
typedef struct {
    int video_width;           /**< 视频宽度 */
    int video_height;          /**< 视频高度 */
    int64_t duration_ms;       /**< 媒体总时长（毫秒），直播流为 -1 */
    int audio_channels;        /**< 音频声道数 */
    int audio_sample_rate;     /**< 音频采样率 (Hz) */
} PrismMediaInfo;

/* ========== 房间信息 ========== */
typedef struct {
    char room_id[64];          /**< 房间 ID */
    char room_name[128];       /**< 房间名称 */
    int member_count;          /**< 当前房间人数 */
    char now_playing[256];     /**< 房间当前播放的媒体名称 */
} PrismRoomInfo;

/* ========== 登录参数 ========== */
typedef struct {
    const char* username;      /**< 用户名 */
    const char* password;      /**< 密码（明文，传输层 TLS 加密） */
    const char* server_url;    /**< 服务器地址，含端口 */
} PrismLoginParams;

#ifdef __cplusplus
}
#endif
