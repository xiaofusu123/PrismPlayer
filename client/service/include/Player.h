#pragma once

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 生命周期 ========== */

/**
 * @brief 创建播放器实例
 * @param config 播放器配置（可为 NULL，使用默认值）
 * @param callback 事件回调函数（可为 NULL）
 * @param user_data 回调透传的用户数据
 * @return 播放器句柄，失败返回 NULL
 */
PrismPlayerHandle prism_player_create(const PrismConfig* config,
                                      PrismEventCallback callback,
                                      void* user_data);

/**
 * @brief 销毁播放器实例，释放所有资源
 * @param player 播放器句柄（传入 NULL 无操作）
 */
void prism_player_destroy(PrismPlayerHandle player);

/* ========== 媒体源 ========== */

/**
 * @brief 打开媒体源（本地文件路径或网络 URL）
 *        调用后状态变为 PRISM_STATE_LOADING，
 *        加载完成后触发 PRISM_EVENT_MEDIA_LOADED 回调
 * @param player 播放器句柄
 * @param uri 文件路径或网络 URL
 * @return PRISM_OK 成功开始加载，否则返回错误码
 */
int prism_player_open(PrismPlayerHandle player, const char* uri);

/**
 * @brief 关闭当前媒体，释放解码资源
 *        状态回到 PRISM_STATE_IDLE
 * @param player 播放器句柄
 * @return PRISM_OK
 */
int prism_player_close(PrismPlayerHandle player);

/* ========== 播放控制 ========== */

/**
 * @brief 开始/恢复播放
 *        状态变为 PRISM_STATE_PLAYING
 *        播放完毕后触发 PRISM_EVENT_PLAYBACK_COMPLETED，状态回到 STOPPED
 * @param player 播放器句柄
 * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA
 */
int prism_player_play(PrismPlayerHandle player);

/**
 * @brief 暂停播放，保留解码资源
 *        状态变为 PRISM_STATE_PAUSED
 * @param player 播放器句柄
 * @return PRISM_OK
 */
int prism_player_pause(PrismPlayerHandle player);

/**
 * @brief 停止播放并释放解码资源
 *        状态变为 PRISM_STATE_STOPPED
 *        与 close 的区别：stop 保留媒体信息，可再次 play 从头播放
 * @param player 播放器句柄
 * @return PRISM_OK
 */
int prism_player_stop(PrismPlayerHandle player);

/**
 * @brief 跳转到指定位置
 *        完成后触发 PRISM_EVENT_SEEK_COMPLETED
 * @param player 播放器句柄
 * @param position_ms 目标位置（毫秒）
 * @param mode PRISM_SEEK_ABSOLUTE（绝对）或 PRISM_SEEK_RELATIVE（相对偏移）
 * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA，
 *         不可 seek 返回 PRISM_ERROR_SEEK_FAILED
 */
int prism_player_seek(PrismPlayerHandle player, int64_t position_ms,
                      PrismSeekMode mode);

/* ========== 状态查询 ========== */

/**
 * @brief 获取当前播放状态
 * @param player 播放器句柄
 * @return 当前 PrismState，player 为 NULL 返回 PRISM_STATE_ERROR
 */
PrismState prism_player_get_state(PrismPlayerHandle player);

/**
 * @brief 获取当前播放位置
 * @param player 播放器句柄
 * @return 当前位置（毫秒），无媒体返回 -1
 */
int64_t prism_player_get_position(PrismPlayerHandle player);

/**
 * @brief 获取媒体总时长
 * @param player 播放器句柄
 * @return 总时长（毫秒），直播流/无媒体返回 -1
 */
int64_t prism_player_get_duration(PrismPlayerHandle player);

/* ========== 音量控制 ========== */

/**
 * @brief 设置音量
 * @param player 播放器句柄
 * @param volume 音量 0.0-1.0，超出范围自动钳位
 * @return PRISM_OK
 */
int prism_player_set_volume(PrismPlayerHandle player, float volume);

/**
 * @brief 获取当前音量
 * @param player 播放器句柄
 * @return 音量值 0.0-1.0，无效句柄返回 0.0
 */
float prism_player_get_volume(PrismPlayerHandle player);

/**
 * @brief 设置静音状态
 * @param player 播放器句柄
 * @param mute true=静音 false=取消静音
 * @return PRISM_OK
 */
int prism_player_set_mute(PrismPlayerHandle player, bool mute);

/**
 * @brief 获取静音状态
 * @param player 播放器句柄
 * @return 是否静音，无效句柄返回 false
 */
bool prism_player_get_mute(PrismPlayerHandle player);

/* ========== 播放属性 ========== */

/**
 * @brief 设置播放速度
 * @param player 播放器句柄
 * @param speed 速度倍率，范围 0.5x-2.0x，1.0 为正常速度
 *              超出范围自动钳位
 * @return PRISM_OK
 */
int prism_player_set_playback_speed(PrismPlayerHandle player, float speed);

/**
 * @brief 获取播放速度
 * @param player 播放器句柄
 * @return 当前速度倍率，无效句柄返回 1.0
 */
float prism_player_get_playback_speed(PrismPlayerHandle player);

/**
 * @brief 设置循环播放
 * @param player 播放器句柄
 * @param loop true=单曲循环 false=播完停止
 * @return PRISM_OK
 */
int prism_player_set_loop(PrismPlayerHandle player, bool loop);

/**
 * @brief 获取循环状态
 * @param player 播放器句柄
 * @return 是否循环播放，无效句柄返回 false
 */
bool prism_player_get_loop(PrismPlayerHandle player);

/* ========== 视频窗口 ========== */

/**
 * @brief 设置视频渲染目标窗口
 * @param player 播放器句柄
 * @param native_window 原生窗口句柄（Windows: HWND 转换为 void*）
 *                      传入 NULL 可解除绑定
 * @return PRISM_OK
 */
int prism_player_set_video_window(PrismPlayerHandle player, void* native_window);

/* ========== 媒体信息 ========== */

/**
 * @brief 获取已加载媒体的详细信息
 *        应在收到 PRISM_EVENT_MEDIA_LOADED 之后调用
 * @param player 播放器句柄
 * @param info [out] 输出参数，调用方分配内存
 * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA
 */
int prism_player_get_media_info(PrismPlayerHandle player, PrismMediaInfo* info);

/* ========== 诊断工具 ========== */

/**
 * @brief 获取最后一次失败操作的详细错误码
 * @param player 播放器句柄
 * @return PrismErrorCode
 */
PrismErrorCode prism_player_get_last_error(PrismPlayerHandle player);

/**
 * @brief 获取 SDK 版本字符串
 * @return 版本号，格式 "PrismPlayer x.y.z"
 */
const char* prism_player_get_version(void);

#ifdef __cplusplus
}
#endif
