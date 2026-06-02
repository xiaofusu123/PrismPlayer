#pragma once

#include "API.h"

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
    PRISM_EVENT_MEDIA_LOADED = 0,
    PRISM_EVENT_PLAYBACK_COMPLETED,
    PRISM_EVENT_SEEK_COMPLETED,
    PRISM_EVENT_BUFFERING_START,
    PRISM_EVENT_BUFFERING_END,
    PRISM_EVENT_ERROR
} PrismEventType;

typedef void (*PrismEventCallback)(PrismEventType type, const void* data, void* user_data);

/* ========== 错误码 ========== */
typedef enum {
    PRISM_OK                   =  0,
    PRISM_ERROR_UNKNOWN        = -1,
    PRISM_ERROR_INVALID_HANDLE = -2,
    PRISM_ERROR_INVALID_PARAM  = -3,
    PRISM_ERROR_NO_MEDIA       = -4,
    PRISM_ERROR_OPEN_FAILED    = -5,
    PRISM_ERROR_SEEK_FAILED    = -6,
    PRISM_ERROR_NOT_SUPPORTED  = -7
} PrismErrorCode;

/* ========== Seek 模式 ========== */
typedef enum {
    PRISM_SEEK_ABSOLUTE = 0,
    PRISM_SEEK_RELATIVE = 1
} PrismSeekMode;

/* ========== 播放配置 ========== */
typedef struct PrismConfig {
    int video_output_width{0};
    int video_output_height{0};
    int audio_sample_rate{0};
    float default_volume{1.0f};
    bool enable_video{true};
    bool enable_audio{true};
    const char* log_level{nullptr};
} PrismConfig;

/* ========== 媒体信息 ========== */
typedef struct PrismMediaInfo {
    int video_width{0};
    int video_height{0};
    int64_t duration_ms{0};
    int audio_channels{0};
    int audio_sample_rate{0};
} PrismMediaInfo;

#ifdef __cplusplus
}
#endif
