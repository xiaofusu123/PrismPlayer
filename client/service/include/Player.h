#pragma once

#include "Types.h"
#include "API.h"

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
_API PrismPlayerHandle prism_player_create(const PrismConfig* config,
                                           PrismEventCallback callback,
                                           void* user_data);

/**
 * @brief 销毁播放器实例，释放所有资源
 *        自动断开网络连接并离开房间
 * @param player 播放器句柄（传入 NULL 无操作）
 */
_API void prism_player_destroy(PrismPlayerHandle player);

/* ========== 用户认证 ========== */

/**
 * @brief 登录服务器进行身份认证
 *        登录完成后触发 PRISM_EVENT_LOGIN_SUCCESS 或 PRISM_EVENT_LOGIN_FAILED 回调
 * @param player 播放器句柄
 * @param params 登录参数（用户名、密码、服务器地址）
 * @return PRISM_OK 成功发起登录请求
 */
_API int prism_player_login(PrismPlayerHandle player, const PrismLoginParams* params);

/**
 * @brief 登出并断开与服务器的连接
 *        若当前在房间内则自动离开房间
 * @param player 播放器句柄
 * @return PRISM_OK
 */
_API int prism_player_logout(PrismPlayerHandle player);

/* ========== 房间管理 ========== */

/**
 * @brief 加入指定房间，建立 WebSocket 连接
 *        加入成功后触发 PRISM_EVENT_ROOM_JOINED 回调
 *        必须在 login 成功后调用
 * @param player 播放器句柄
 * @param room_id 房间 ID 字符串
 * @return PRISM_OK，未登录返回 PRISM_ERROR_NETWORK
 */
_API int prism_player_join_room(PrismPlayerHandle player, const char* room_id);

/**
 * @brief 离开当前房间，断开房间级 WebSocket
 *        离开后触发 PRISM_EVENT_ROOM_LEFT 回调
 * @param player 播放器句柄
 * @return PRISM_OK
 */
_API int prism_player_leave_room(PrismPlayerHandle player);

/**
 * @brief 获取当前房间信息
 * @param player 播放器句柄
 * @param info [out] 输出参数，调用方分配内存
 * @return PRISM_OK，未在房间中返回 PRISM_ERROR_NETWORK
 */
_API int prism_player_get_room_info(PrismPlayerHandle player, PrismRoomInfo* info);

/**
 * @brief 获取当前房间连接状态
 * @param player 播放器句柄
 * @return 当前 PrismRoomState
 */
_API PrismRoomState prism_player_get_room_state(PrismPlayerHandle player);

/* ========== 媒体源 ========== */

/**
 * @brief 打开媒体源（本地文件路径或网络 URL）
 *        调用后状态变为 PRISM_STATE_LOADING，
 *        加载完成后触发 PRISM_EVENT_MEDIA_LOADED 回调
 * @param player 播放器句柄
 * @param uri 文件路径或网络 URL
 * @return PRISM_OK 成功开始加载，否则返回错误码
 */
_API int prism_player_open(PrismPlayerHandle player, const char* uri);

/**
 * @brief 关闭当前媒体，释放解码资源
 *        状态回到 PRISM_STATE_IDLE
 * @param player 播放器句柄
 * @return PRISM_OK
 */
_API int prism_player_close(PrismPlayerHandle player);

/* ========== 播放控制 ========== */

/**
 * @brief 开始/恢复播放
 *        状态变为 PRISM_STATE_PLAYING
 *        播放完毕后触发 PRISM_EVENT_PLAYBACK_COMPLETED，状态回到 STOPPED
 * @param player 播放器句柄
 * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA
 */
_API int prism_player_play(PrismPlayerHandle player);

/**
 * @brief 暂停播放，保留解码资源
 *        状态变为 PRISM_STATE_PAUSED
 * @param player 播放器句柄
 * @return PRISM_OK
 */
_API int prism_player_pause(PrismPlayerHandle player);

/**
 * @brief 停止播放并释放解码资源
 *        状态变为 PRISM_STATE_STOPPED
 *        与 close 的区别：stop 保留媒体信息，可再次 play 从头播放
 * @param player 播放器句柄
 * @return PRISM_OK
 */
_API int prism_player_stop(PrismPlayerHandle player);

/**
 * @brief 跳转到指定位置
 *        完成后触发 PRISM_EVENT_SEEK_COMPLETED
 * @param player 播放器句柄
 * @param position_ms 目标位置（毫秒）
 * @param mode PRISM_SEEK_ABSOLUTE（绝对）或 PRISM_SEEK_RELATIVE（相对偏移）
 * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA，
 *         不可 seek 返回 PRISM_ERROR_SEEK_FAILED
 */
_API int prism_player_seek(PrismPlayerHandle player, int64_t position_ms,
                           PrismSeekMode mode);

/* ========== 状态查询 ========== */

/**
 * @brief 获取当前播放状态
 * @param player 播放器句柄
 * @return 当前 PrismState，player 为 NULL 返回 PRISM_STATE_ERROR
 */
_API PrismState prism_player_get_state(PrismPlayerHandle player);

/**
 * @brief 获取当前播放位置
 * @param player 播放器句柄
 * @return 当前位置（毫秒），无媒体返回 -1
 */
_API int64_t prism_player_get_position(PrismPlayerHandle player);

/**
 * @brief 获取媒体总时长
 * @param player 播放器句柄
 * @return 总时长（毫秒），直播流/无媒体返回 -1
 */
_API int64_t prism_player_get_duration(PrismPlayerHandle player);

/* ========== 音量控制 ========== */

/**
 * @brief 设置音量
 * @param player 播放器句柄
 * @param volume 音量 0.0-1.0，超出范围自动钳位
 * @return PRISM_OK
 */
_API int prism_player_set_volume(PrismPlayerHandle player, float volume);

/**
 * @brief 获取当前音量
 * @param player 播放器句柄
 * @return 音量值 0.0-1.0，无效句柄返回 0.0
 */
_API float prism_player_get_volume(PrismPlayerHandle player);

/**
 * @brief 设置静音状态
 * @param player 播放器句柄
 * @param mute true=静音 false=取消静音
 * @return PRISM_OK
 */
_API int prism_player_set_mute(PrismPlayerHandle player, bool mute);

/**
 * @brief 获取静音状态
 * @param player 播放器句柄
 * @return 是否静音，无效句柄返回 false
 */
_API bool prism_player_get_mute(PrismPlayerHandle player);

/* ========== 播放属性 ========== */

/**
 * @brief 设置播放速度
 * @param player 播放器句柄
 * @param speed 速度倍率，范围 0.5x-2.0x，1.0 为正常速度
 *              超出范围自动钳位
 * @return PRISM_OK
 */
_API int prism_player_set_playback_speed(PrismPlayerHandle player, float speed);

/**
 * @brief 获取播放速度
 * @param player 播放器句柄
 * @return 当前速度倍率，无效句柄返回 1.0
 */
_API float prism_player_get_playback_speed(PrismPlayerHandle player);

/**
 * @brief 设置循环播放
 * @param player 播放器句柄
 * @param loop true=单曲循环 false=播完停止
 * @return PRISM_OK
 */
_API int prism_player_set_loop(PrismPlayerHandle player, bool loop);

/**
 * @brief 获取循环状态
 * @param player 播放器句柄
 * @return 是否循环播放，无效句柄返回 false
 */
_API bool prism_player_get_loop(PrismPlayerHandle player);

/* ========== 视频窗口 ========== */

/**
 * @brief 设置视频渲染目标窗口
 * @param player 播放器句柄
 * @param native_window 原生窗口句柄（Windows: HWND 转换为 void*）
 *                      传入 NULL 可解除绑定
 * @return PRISM_OK
 */
_API int prism_player_set_video_window(PrismPlayerHandle player, void* native_window);

/* ========== 媒体信息 ========== */

/**
 * @brief 获取已加载媒体的详细信息
 *        应在收到 PRISM_EVENT_MEDIA_LOADED 之后调用
 * @param player 播放器句柄
 * @param info [out] 输出参数，调用方分配内存
 * @return PRISM_OK，未加载媒体返回 PRISM_ERROR_NO_MEDIA
 */
_API int prism_player_get_media_info(PrismPlayerHandle player, PrismMediaInfo* info);

/* ========== 诊断工具 ========== */

/**
 * @brief 获取最后一次失败操作的详细错误码
 * @param player 播放器句柄
 * @return PrismErrorCode
 */
_API PrismErrorCode prism_player_get_last_error(PrismPlayerHandle player);

/**
 * @brief 获取 SDK 版本字符串
 * @return 版本号，格式 "PrismPlayer x.y.z"
 */
_API const char* prism_player_get_version(void);

/* ========== 依赖注入（可选，不调用则使用默认实现） ========== */

/**
 * @brief 注入音频引擎工厂
 *        应在 prism_player_create 之后、首次 open/play 之前调用
 * @param player 播放器句柄
 * @param factory AudioEngineFactory 指针（void* 以保持 C ABI 纯净）
 * @return PRISM_OK
 */
_API int prism_player_set_audio_factory(PrismPlayerHandle player, void* factory);

/**
 * @brief 注入视频引擎工厂
 *        应在 prism_player_create 之后、首次 open/play 之前调用
 * @param player 播放器句柄
 * @param factory VideoEngineFactory 指针（void* 以保持 C ABI 纯净）
 * @return PRISM_OK
 */
_API int prism_player_set_video_factory(PrismPlayerHandle player, void* factory);

/**
 * @brief 注入网络客户端实现
 *        应在 prism_player_create 之后、login 之前调用
 * @param player 播放器句柄
 * @param network IServiceNetwork 指针（void* 以保持 C ABI 纯净）
 * @return PRISM_OK
 */
_API int prism_player_set_network_client(PrismPlayerHandle player, void* network);

#ifdef __cplusplus
}
#endif
