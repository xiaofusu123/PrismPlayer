# PrismPlayer API 接口文档

> 基于项目实际代码生成，最后更新：2026-06-08
> 版本：0.0.1

---

## 1. Service 层 — 公开 C API

> 头文件：`client/service/include/Player.h`、`client/service/include/Types.h`
> 命名空间：`extern "C"`
> 构建产物：动态库 (`client-service`)
> 状态：完整实现（27 个 C API）

### 1.1 版本

| 宏 | 值 |
|----|-----|
| `PRISM_VERSION_MAJOR` | 0 |
| `PRISM_VERSION_MINOR` | 0 |
| `PRISM_VERSION_PATCH` | 1 |

### 1.2 通用类型

**句柄**

| 类型 | 定义 |
|------|------|
| `PrismPlayerHandle` | `void*` —— 不透明句柄，所有 API 的第一个参数 |

**播放状态 `PrismState`**

| 值 | 说明 |
|----|------|
| `PRISM_STATE_IDLE = 0` | 空闲状态，未加载媒体 |
| `PRISM_STATE_LOADING` | 媒体加载中 |
| `PRISM_STATE_PLAYING` | 播放中 |
| `PRISM_STATE_PAUSED` | 已暂停 |
| `PRISM_STATE_STOPPED` | 已停止 |
| `PRISM_STATE_ERROR` | 错误状态 |

**事件类型 `PrismEventType`**

| 值 | 触发时机 |
|----|----------|
| `PRISM_EVENT_MEDIA_LOADED = 0` | 媒体加载完成 |
| `PRISM_EVENT_PLAYBACK_COMPLETED` | 播放完成 |
| `PRISM_EVENT_SEEK_COMPLETED` | Seek 操作完成 |
| `PRISM_EVENT_BUFFERING_START` | 缓冲开始 |
| `PRISM_EVENT_BUFFERING_END` | 缓冲结束 |
| `PRISM_EVENT_ERROR` | 发生错误 |
| `PRISM_EVENT_LOGIN_SUCCESS` | 登录成功 |
| `PRISM_EVENT_LOGIN_FAILED` | 登录失败 |
| `PRISM_EVENT_ROOM_JOINED` | 成功加入房间 |
| `PRISM_EVENT_ROOM_LEFT` | 已离开房间 |

**错误码 `PrismErrorCode`**

| 值 | 数值 | 说明 |
|----|------|------|
| `PRISM_OK` | 0 | 操作成功 |
| `PRISM_ERROR_UNKNOWN` | -1 | 未知错误 |
| `PRISM_ERROR_INVALID_HANDLE` | -2 | 无效的播放器句柄 |
| `PRISM_ERROR_INVALID_PARAM` | -3 | 无效的参数 |
| `PRISM_ERROR_NO_MEDIA` | -4 | 未加载媒体 |
| `PRISM_ERROR_OPEN_FAILED` | -5 | 打开媒体失败 |
| `PRISM_ERROR_SEEK_FAILED` | -6 | Seek 操作失败 |
| `PRISM_ERROR_NOT_SUPPORTED` | -7 | 不支持的操作 |

**Seek 模式 `PrismSeekMode`**

| 值 | 说明 |
|----|------|
| `PRISM_SEEK_ABSOLUTE = 0` | 绝对跳转，参数为目标位置 |
| `PRISM_SEEK_RELATIVE = 1` | 相对跳转，参数为偏移量 |

**房间连接状态 `PrismRoomState`**

| 值 | 说明 |
|----|------|
| `PRISM_ROOM_DISCONNECTED = 0` | 未连接 |
| `PRISM_ROOM_CONNECTING` | 连接中 |
| `PRISM_ROOM_CONNECTED` | 已连接 |

**事件回调**

| 签名 | 说明 |
|------|------|
| `void (*PrismEventCallback)(PrismEventType type, const void* data, void* user_data)` | type=事件类型；data=附加数据(可为NULL)；user_data=注册时透传 |

### 1.3 结构体

**PrismConfig — 播放器初始化配置**

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `video_output_width` | `int` | 0 | 视频输出宽度（像素），0 表示使用原始分辨率 |
| `video_output_height` | `int` | 0 | 视频输出高度（像素），0 表示使用原始分辨率 |
| `audio_sample_rate` | `int` | 0 | 音频采样率（Hz），0 表示使用原始采样率 |
| `default_volume` | `float` | 1.0 | 默认音量，范围 0.0-1.0 |
| `enable_video` | `bool` | true | 是否启用视频渲染 |
| `enable_audio` | `bool` | true | 是否启用音频输出 |
| `log_level` | `const char*` | `"info"` | spdlog 日志等级 |

**PrismMediaInfo — 媒体详细信息**

| 字段 | 类型 | 说明 |
|------|------|------|
| `video_width` | `int` | 视频宽度（像素），无视频流时为 0 |
| `video_height` | `int` | 视频高度（像素），无视频流时为 0 |
| `duration_ms` | `int64_t` | 媒体总时长（毫秒），直播流为 -1 |
| `audio_channels` | `int` | 音频声道数，无音频流时为 0 |
| `audio_sample_rate` | `int` | 音频采样率（Hz），无音频流时为 0 |

**PrismLoginParams — 登录认证参数**

| 字段 | 类型 | 说明 |
|------|------|------|
| `username` | `const char*` | 用户名 |
| `password` | `const char*` | 密码 |
| `server_url` | `const char*` | 服务器地址 |

**PrismRoomInfo — 房间信息**

| 字段 | 类型 | 说明 |
|------|------|------|
| `room_id` | `char[256]` | 房间 ID |
| `member_count` | `int` | 房间在线人数 |

---

### 1.4 生命周期

| 签名 | 说明 |
|------|------|
| `PrismPlayerHandle prism_player_create(const PrismConfig* config, PrismEventCallback callback, void* user_data)` | 创建播放器实例。config 为 NULL 则使用默认值；callback 为 NULL 则不接收事件。失败返回 NULL |
| `void prism_player_destroy(PrismPlayerHandle player)` | 销毁播放器实例，释放所有资源。自动断开网络连接并离开房间。传入 NULL 无操作 |

### 1.5 用户认证

| 签名 | 说明 |
|------|------|
| `int prism_player_login(PrismPlayerHandle player, const PrismLoginParams* params)` | 登录服务器进行身份认证。完成后触发 `LOGIN_SUCCESS` 或 `LOGIN_FAILED` 回调 |
| `int prism_player_logout(PrismPlayerHandle player)` | 登出并断开与服务器的连接。若当前在房间内则自动离开房间 |

### 1.6 房间管理

| 签名 | 说明 |
|------|------|
| `int prism_player_join_room(PrismPlayerHandle player, const char* room_id)` | 加入指定房间，建立 WebSocket 连接。必须在 login 成功后调用。成功后触发 `ROOM_JOINED` |
| `int prism_player_leave_room(PrismPlayerHandle player)` | 离开当前房间，断开房间级 WebSocket。触发 `ROOM_LEFT` |
| `int prism_player_get_room_info(PrismPlayerHandle player, PrismRoomInfo* info)` | 获取当前房间信息。调用方分配 `PrismRoomInfo` 内存。未在房间中返回错误 |
| `PrismRoomState prism_player_get_room_state(PrismPlayerHandle player)` | 获取当前房间连接状态 |

### 1.7 媒体源

| 签名 | 说明 |
|------|------|
| `int prism_player_open(PrismPlayerHandle player, const char* uri)` | 打开媒体源（本地文件路径或网络 URL）。状态变为 `LOADING`，完成后触发 `MEDIA_LOADED` |
| `int prism_player_close(PrismPlayerHandle player)` | 关闭当前媒体，释放解码资源。状态回到 `IDLE` |

### 1.8 播放控制

| 签名 | 说明 |
|------|------|
| `int prism_player_play(PrismPlayerHandle player)` | 开始/恢复播放。状态变为 `PLAYING`。播放完毕后触发 `PLAYBACK_COMPLETED`，状态回到 `STOPPED` |
| `int prism_player_pause(PrismPlayerHandle player)` | 暂停播放，保留解码资源。状态变为 `PAUSED` |
| `int prism_player_stop(PrismPlayerHandle player)` | 停止播放并释放解码资源。状态变为 `STOPPED`。与 close 的区别：stop 保留媒体信息，可再次 play 从头播放 |
| `int prism_player_seek(PrismPlayerHandle player, int64_t position_ms, PrismSeekMode mode)` | 跳转到指定位置。完成后触发 `SEEK_COMPLETED`。未加载媒体返回 `NO_MEDIA` |

### 1.9 状态查询

| 签名 | 说明 |
|------|------|
| `PrismState prism_player_get_state(PrismPlayerHandle player)` | 获取当前播放状态。player 为 NULL 返回 `ERROR` |
| `int64_t prism_player_get_position(PrismPlayerHandle player)` | 获取当前播放位置（毫秒）。无媒体返回 -1 |
| `int64_t prism_player_get_duration(PrismPlayerHandle player)` | 获取媒体总时长（毫秒）。直播流或无媒体返回 -1 |

### 1.10 音量控制

| 签名 | 说明 |
|------|------|
| `int prism_player_set_volume(PrismPlayerHandle player, float volume)` | 设置音量 0.0-1.0，超出范围自动钳位 |
| `float prism_player_get_volume(PrismPlayerHandle player)` | 获取当前音量 0.0-1.0。无效句柄返回 0.0 |
| `int prism_player_set_mute(PrismPlayerHandle player, bool mute)` | 设置静音状态 |
| `bool prism_player_get_mute(PrismPlayerHandle player)` | 获取静音状态。无效句柄返回 false |

### 1.11 播放属性

| 签名 | 说明 |
|------|------|
| `int prism_player_set_playback_speed(PrismPlayerHandle player, float speed)` | 设置播放速度，范围 0.5x-2.0x，超出自动钳位 |
| `float prism_player_get_playback_speed(PrismPlayerHandle player)` | 获取当前速度倍率。无效句柄返回 1.0 |
| `int prism_player_set_loop(PrismPlayerHandle player, bool loop)` | 设置循环播放。true=单曲循环，false=播完停止 |
| `bool prism_player_get_loop(PrismPlayerHandle player)` | 获取循环状态。无效句柄返回 false |

### 1.12 视频窗口

| 签名 | 说明 |
|------|------|
| `int prism_player_set_video_window(PrismPlayerHandle player, void* native_window)` | 设置视频渲染目标窗口。Windows 下传入 HWND 转 void*；传入 NULL 解除绑定 |

### 1.13 媒体信息

| 签名 | 说明 |
|------|------|
| `int prism_player_get_media_info(PrismPlayerHandle player, PrismMediaInfo* info)` | 获取已加载媒体的详细信息。应在收到 `MEDIA_LOADED` 后调用。调用方分配内存 |

### 1.14 诊断工具

| 签名 | 说明 |
|------|------|
| `PrismErrorCode prism_player_get_last_error(PrismPlayerHandle player)` | 获取最后一次失败操作的详细错误码 |
| `const char* prism_player_get_version(void)` | 获取 SDK 版本字符串，格式 `"PrismPlayer x.y.z"` |

### 1.15 依赖注入（可选）

| 签名 | 说明 |
|------|------|
| `int prism_player_set_audio_factory(PrismPlayerHandle player, void* factory)` | 注入音频引擎工厂。应在 create 之后、首次 open/play 之前调用。不调用则使用默认实现 |
| `int prism_player_set_video_factory(PrismPlayerHandle player, void* factory)` | 注入视频引擎工厂。应在 create 之后、首次 open/play 之前调用。不调用则使用默认实现 |
| `int prism_player_set_network_client(PrismPlayerHandle player, void* network)` | 注入网络客户端实现。应在 create 之后、login 之前调用。不调用则使用默认实现 |

> 参数使用 `void*` 以保持 C ABI 纯净，实际类型为 `AudioEngineFactory*` / `VideoEngineFactory*` / `IServiceNetwork*`。

---

## 2. Engine 层 API

> 命名空间：`Prism::Engine`
> 头文件：`client/engine/include/`
> 状态：抽象接口 + 工厂骨架完成，解码模块待实现
> 调用方：Business 层

### 2.1 同步数据结构

**AudioSyncInfo — 音频同步信息**

| 字段 | 类型 | 说明 |
|------|------|------|
| `current_pts` | `std::atomic<uint64_t>` | 当前音频帧解码时间戳（ms） |
| `next_pts` | `std::atomic<uint64_t>` | 下一帧音频解码时间戳（ms） |
| `start_time` | `std::atomic<uint32_t>` | 流开始的时间 |
| `duration` | `std::atomic<uint64_t>` | 音频总时长 |

**VideoSyncInfo — 视频同步信息**

| 字段 | 类型 | 说明 |
|------|------|------|
| `current_pts` | `std::atomic<uint64_t>` | 当前视频帧解码时间戳（ms） |
| `next_pts` | `std::atomic<uint64_t>` | 下一帧视频解码时间戳（ms） |
| `start_time` | `std::atomic<uint32_t>` | 流开始的时间 |
| `duration` | `std::atomic<uint64_t>` | 视频总时长 |

**RenderMetadata — 渲染元数据**

| 字段 | 类型 | 说明 |
|------|------|------|
| `handle` | `RENDER_RESULT_HANDLE` | 渲染结果句柄（Windows: HANDLE） |
| `valid` | `bool` | 句柄是否有效 |
| `width` | `uint32_t` | 渲染宽度 |
| `height` | `uint32_t` | 渲染高度 |
| `format` | `uint32_t` | 像素格式 |
| `timestamp` | `uint64_t` | 时间戳 |

> 以上结构体均使用 `std::atomic` 保证跨线程读写安全，默认值为 0。

### 2.2 AudioEngine — 音频引擎抽象接口

| 方法 | 返回 | 说明 |
|------|------|------|
| `init()` | `bool` | 初始化音频引擎 |
| `play()` | `bool` | 播放音频 |
| `pause()` | `bool` | 暂停音频 |
| `close()` | `bool` | 关闭音频，释放资源 |
| `set_play_speed()` | `bool` | 设置播放速度（倍速播放） |
| `seek(uint64_t pts, int seek_mode)` | `bool` | 音频跳转。seek_mode: 0=绝对模式(pts为非负目标位置)，1=相对模式(快进/倒退) |
| `get_sync_info()` | `AudioSyncInfo` | 获取音频同步信息 |

### 2.3 AudioEngineFactory — 音频引擎工厂

| 方法 | 返回 | 说明 |
|------|------|------|
| `create_audio_engine()` | `std::unique_ptr<AudioEngine>` | 创建音频引擎实例 |

**平台实现：**

| 工厂类 | 引擎类 | 平台 |
|--------|--------|------|
| `AudioEngineWasapiSharedFactory` | `AudioEngineWasapiShared` | Windows (WASAPI 共享模式) |
| `AudioEngineAAudio`（计划） | — | Android |
| `AudioEngineAudioUnit`（计划） | — | iOS/macOS |

### 2.4 VideoEngine — 视频引擎抽象接口

| 方法 | 返回 | 说明 |
|------|------|------|
| `init()` | `bool` | 初始化视频引擎 |
| `play()` | `bool` | 播放视频 |
| `pause()` | `bool` | 暂停视频 |
| `close()` | `bool` | 关闭视频 |
| `set_play_speed(float speed)` | `bool` | 设置播放速度（倍速播放） |
| `seek(uint64_t pts, int seek_mode)` | `bool` | 视频跳转。seek_mode: 0=绝对模式，1=相对模式 |
| `get_sync_info()` | `VideoSyncInfo` | 获取视频同步信息 |
| `get_render_result()` | `RenderMetadata` | 获取视频渲染结果（纹理句柄+元数据） |

### 2.5 VideoEngineFactory — 视频引擎工厂

| 方法 | 返回 | 说明 |
|------|------|------|
| `create_video_engine()` | `std::unique_ptr<VideoEngine>` | 创建视频引擎实例 |

**平台实现：**

| 工厂类 | 引擎类 | 平台 |
|--------|--------|------|
| `VideoEngineVulkanFactory` | `VideoEngineVulkan` | 跨平台 (Vulkan) |

---

## 3. Business 层 API

> 命名空间：`Prism::Business`
> 头文件：`client/business/{av_sync,network}/include/`
> 状态：AV Sync 完整实现，Network 接口已定义
> 调用方：Service 层

### 3.1 AV Sync 模块 — 音视频同步

#### 同步类型定义

**SyncState — 同步状态**

以音频时钟为主时钟的七状态模型：

| 值 | 说明 |
|----|------|
| `UNINIT` | 音频引擎未初始化 |
| `CALIBATING` | 音频校准中（初始缓冲阶段） |
| `SYNCHRONIZED` | 音频已同步 |
| `AHEAD` | 音频时间超前 |
| `BEHIND` | 音频时间滞后 |
| `DISABLE` | 未启用同步控制（纯本地播放模式） |
| `SYNC_ERROR` | 错误状态 |

状态流转：`UNINIT → CALIBATING → SYNCHRONIZED`，`SYNCHRONIZED` 可切换至 `AHEAD`/`BEHIND`。

**SyncAction — 同步算法渲染决策**

| 值 | 说明 |
|----|------|
| `RENDER` | 正常渲染当前帧 |
| `WAIT` | 视频超前，等待音频追赶 |
| `DROP` | 视频滞后，丢弃当前帧 |

**SyncConfig — 同步算法配置**

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `ahead_threshold_ms` | `uint32_t` | 30 | 视频超前超过此值则 WAIT（ms） |
| `behind_threshold_ms` | `uint32_t` | 50 | 视频滞后超过此值则 DROP（ms） |
| `calibrate_duration_ms` | `uint32_t` | 500 | 初始校准缓冲时长（ms） |

**DriftInfo — 漂移信息**

| 字段 | 类型 | 说明 |
|------|------|------|
| `drift_ms` | `int64_t` | 音视频漂移值（ms），正数=视频超前 |
| `audio_pts` | `uint64_t` | 当前音频 PTS（ms） |
| `video_pts` | `uint64_t` | 当前视频 PTS（ms） |

---

#### IPlaybackStateMachine — 播放状态机

控制播放/暂停/Seek 状态转换，校验转换合法性。

| 方法 | 返回 | 说明 |
|------|------|------|
| `transition(SyncState target)` | `bool` | 状态转换，非法转换返回 false |
| `get_state()` | `SyncState` | 获取当前同步状态 |
| `can_play()` | `bool` | 当前状态是否允许播放 |
| `can_seek()` | `bool` | 当前状态是否允许跳转 |
| `reset()` | `void` | 重置状态机到 UNINIT |

#### ISyncAlgorithm — 音视频同步算法

以音频时钟为主时钟，计算视频帧与音频时钟的漂移值，输出渲染决策。

| 方法 | 返回 | 说明 |
|------|------|------|
| `calibrate(uint64_t audio_pts, uint64_t video_pts)` | `SyncAction` | 校准音视频同步，计算漂移并输出决策 |
| `configure(const SyncConfig& config)` | `void` | 配置同步参数（阈值、缓冲时长） |
| `get_drift_info()` | `DriftInfo` | 获取当前漂移信息 |
| `reset()` | `void` | 重置同步算法状态 |

#### ICommandDispatcher — 指令分发器

封装对 AudioEngine/VideoEngine 的直接调用，使上层无需直接操作 Engine。

| 方法 | 返回 | 说明 |
|------|------|------|
| `dispatch_play()` | `bool` | 下发播放指令 |
| `dispatch_pause()` | `bool` | 下发暂停指令 |
| `dispatch_seek(uint64_t pts, int seek_mode)` | `bool` | 下发跳转指令。mode: 0=绝对，1=相对 |
| `dispatch_speed(float speed)` | `bool` | 下发倍速指令 |
| `set_audio_engine(AudioEngine* engine)` | `void` | 绑定音频引擎（不持有所有权） |
| `set_video_engine(VideoEngine* engine)` | `void` | 绑定视频引擎（不持有所有权） |
| `initialize_engines(void* audio_factory, void* video_factory, bool enable_video)` | `bool` | 从工厂创建并初始化引擎，接管所有权 |
| `shutdown_engines()` | `void` | 关闭并销毁所有引擎实例 |

#### IEngineObserver — 引擎帧观察者

接收 Engine 层的帧回调，驱动同步算法校准。在帧到达时触发 calibrate。

| 方法 | 返回 | 说明 |
|------|------|------|
| `on_audio_frame(const AudioSyncInfo& info)` | `void` | 音频帧到达回调 |
| `on_video_frame(const VideoSyncInfo& info)` | `void` | 视频帧到达回调 |
| `on_render_ready(const RenderMetadata& metadata)` | `void` | 渲染就绪回调 |
| `set_sync_algorithm(ISyncAlgorithm* algo)` | `void` | 绑定同步算法（不持有所有权） |
| `set_state_machine(IPlaybackStateMachine* sm)` | `void` | 绑定状态机（不持有所有权） |

#### SyncFactory — 工厂函数

| 函数 | 返回 |
|------|------|
| `create_playback_state_machine()` | `std::unique_ptr<IPlaybackStateMachine>` |
| `create_sync_algorithm()` | `std::unique_ptr<ISyncAlgorithm>` |
| `create_command_dispatcher()` | `std::unique_ptr<ICommandDispatcher>` |
| `create_engine_observer()` | `std::unique_ptr<IEngineObserver>` |

---

### 3.2 Network 模块 — 网络通信

#### 网络类型定义

**NetworkState — 连接状态**

| 值 | 说明 |
|----|------|
| `DISCONNECTED` | 未连接 |
| `CONNECTING` | 连接中 |
| `CONNECTED` | 已连接 |
| `ERROR` | 连接异常 |

**LoginResult — 登录结果**

| 值 | 说明 |
|----|------|
| `SUCCESS` | 登录成功 |
| `INVALID_CREDENTIALS` | 用户名或密码错误 |
| `NETWORK_ERROR` | 网络连接失败 |
| `SERVER_ERROR` | 服务器内部错误 |
| `TIMEOUT` | 请求超时 |

**PlaybackCommand — 联机播放指令**

| 值 | 说明 |
|----|------|
| `PLAY` | 播放 |
| `PAUSE` | 暂停 |
| `SEEK` | 跳转 |
| `SPEED` | 变速 |

**RoomInfo — 房间信息**

| 字段 | 类型 | 说明 |
|------|------|------|
| `room_id` | `char[256]` | 房间 ID |
| `member_count` | `int` | 房间在线人数 |

---

#### INetworkObserver — 网络事件观察者

上层（Service）实现此接口以接收网络层的异步事件回调。各网络子模块通过此接口向上通知。

| 回调 | 说明 |
|------|------|
| `on_login_result(LoginResult result)` | 登录结果回调 |
| `on_room_joined(const RoomInfo& info)` | 成功加入房间回调 |
| `on_room_left()` | 离开房间回调 |
| `on_room_message(const char* message)` | 服务器推送消息回调（JSON 字符串） |
| `on_network_error(int error_code, const char* error_msg)` | 网络错误回调 |

#### IAccountManager — 账号认证管理

负责用户登录/登出及登录状态查询。登录为异步操作，结果通过 `on_login_result()` 回调。

| 方法 | 返回 | 说明 |
|------|------|------|
| `login(const char* username, const char* password, const char* server_url)` | `bool` | 发起异步登录请求 |
| `logout()` | `bool` | 登出并断开与服务器的连接 |
| `is_logged_in()` | `bool` | 查询当前登录状态 |
| `set_observer(INetworkObserver* observer)` | `void` | 注册事件观察者（不持有所有权） |

#### IRoomManager — 房间管理

负责房间加入/离开及房间信息查询。加入/离开为异步操作。

| 方法 | 返回 | 说明 |
|------|------|------|
| `join_room(const char* room_id)` | `bool` | 加入指定房间，结果通过 `on_room_joined` 回调 |
| `leave_room()` | `bool` | 离开当前房间，结果通过 `on_room_left` 回调 |
| `get_room_info(RoomInfo* info)` | `bool` | 获取当前房间信息 |
| `get_room_state()` | `NetworkState` | 获取当前房间连接状态 |
| `set_observer(INetworkObserver* observer)` | `void` | 注册事件观察者（不持有所有权） |

#### ISignalingClient — WebSocket 信令客户端

负责与服务器的实时信令通信，包括播放控制同步消息的收发。

| 方法 | 返回 | 说明 |
|------|------|------|
| `connect(const char* url)` | `bool` | 连接到信令服务器 |
| `disconnect()` | `void` | 断开与信令服务器的连接 |
| `send_message(const char* message)` | `bool` | 向服务器发送消息 |
| `get_state()` | `NetworkState` | 获取当前连接状态 |
| `set_observer(INetworkObserver* observer)` | `void` | 注册事件观察者（不持有所有权） |

#### NetworkFactory — 工厂函数

| 函数 | 返回 |
|------|------|
| `create_account_manager()` | `std::unique_ptr<IAccountManager>` |
| `create_room_manager()` | `std::unique_ptr<IRoomManager>` |
| `create_signaling_client()` | `std::unique_ptr<ISignalingClient>` |

---

## 4. Adapter 层 API

> 命名空间：`Prism::Adapter`
> 头文件：`client/adapter/include/`
> 构建产物：静态库 (`client-adapter`)
> 状态：框架已搭好，代码待编写

### 4.1 FileAdapter — 文件 I/O 与媒体元数据提取

**FileMetadata — 媒体文件元数据**

音频/视频字段共存，非适用字段为零值。

| 字段 | 类型 | 说明 |
|------|------|------|
| `file_path` | `std::string` | 文件路径 |
| `file_size` | `uint64_t` | 文件大小（字节） |
| `format` | `std::string` | 容器格式（MP4/MKV/MP3/FLAC 等） |
| `duration` | `uint64_t` | 总时长（ms） |
| `bit_rate` | `uint64_t` | 总比特率（bps） |
| `sample_rate` | `uint32_t` | 采样率（Hz） |
| `channels` | `uint32_t` | 声道数（1/2/6/8） |
| `bit_depth` | `uint32_t` | 位深（16/24/32） |
| `channel_layout` | `std::string` | 声道布局（mono/stereo/5.1/7.1） |
| `resolution_width` | `uint32_t` | 分辨率宽度 |
| `resolution_height` | `uint32_t` | 分辨率高度 |
| `color_depth` | `uint32_t` | 色深（8/10 bit） |
| `frame_rate` | `double` | 帧率（fps） |
| `codec_name` | `std::string` | 编码器名称（H.264/H.265/AV1） |

**接口方法**

| 方法 | 返回 | 说明 |
|------|------|------|
| `open(const std::string& file_path)` | `bool` | 打开文件 |
| `close()` | `void` | 关闭文件 |
| `read(uint8_t* buffer, size_t size)` | `size_t` | 读取数据，返回实际读取字节数 |
| `write(const uint8_t* buffer, size_t size)` | `size_t` | 写入数据，返回实际写入字节数 |
| `seek(int64_t offset, int whence)` | `bool` | 移动文件读写指针 |
| `get_size()` | `uint64_t` | 获取文件大小（字节） |
| `get_metadata()` | `FileMetadata` | 获取媒体文件元数据 |
| `is_open()` | `bool` | 判断文件是否已打开 |

### 4.2 DBAdapter — SQLite 数据库操作

提供表级 CRUD 操作。

| 表 | 结构体 | 主要字段 |
|----|--------|----------|
| `accounts` | `AccountInfo` | id, username, email, password_hash |
| `audio_files` | `AudioFileInfo` | title, file_path, duration, sample_rate, cover_path, lyrics_path... |
| `video_files` | `VideoFileInfo` | filename, file_path, resolution, frame_rate, codec_name... |
| `settings` | 键值对 | key, value |

**接口方法**

| 分组 | 方法 | 返回 | 说明 |
|------|------|------|------|
| 生命周期 | `init(const std::string& db_path)` | `bool` | 初始化数据库连接 |
| | `close()` | `void` | 关闭数据库连接 |
| Account | `insert_account(const AccountInfo& info)` | `bool` | 插入账户 |
| | `query_account_by_username(const std::string&)` | `std::optional<AccountInfo>` | 按用户名查询 |
| | `update_account(const AccountInfo& info)` | `bool` | 更新账户信息 |
| | `delete_account(int64_t id)` | `bool` | 删除账户 |
| Audio | `insert_audio_file(const AudioFileInfo&)` | `bool` | 插入音频文件 |
| | `query_audio_files()` | `std::vector<AudioFileInfo>` | 查询全部音频 |
| | `query_audio_file_by_id(int64_t id)` | `std::optional<AudioFileInfo>` | 按 ID 查询 |
| | `update_audio_file(const AudioFileInfo&)` | `bool` | 更新音频文件 |
| | `delete_audio_file(int64_t id)` | `bool` | 删除音频文件 |
| Video | `insert_video_file(const VideoFileInfo&)` | `bool` | 插入视频文件 |
| | `query_video_files()` | `std::vector<VideoFileInfo>` | 查询全部视频 |
| | `query_video_file_by_id(int64_t id)` | `std::optional<VideoFileInfo>` | 按 ID 查询 |
| | `update_video_file(const VideoFileInfo&)` | `bool` | 更新视频文件 |
| | `delete_video_file(int64_t id)` | `bool` | 删除视频文件 |
| Settings | `get_setting(const std::string& key)` | `std::optional<std::string>` | 获取设置项 |
| | `set_setting(const std::string& key, const std::string& value)` | `bool` | 设置（新增或更新）设置项 |

### 4.3 NetworkAdapter — 网络通信

封装 HTTP 请求与 WebSocket 通信。

**辅助类型**

| 类型 | 说明 |
|------|------|
| `HttpResponse` | HTTP 响应：status_code + body + headers |
| `WsMessageType { TEXT, BINARY }` | WebSocket 消息类型 |
| `WsMessage` | WebSocket 消息：type + data |
| `WsCallback` | WebSocket 事件回调接口：on_connected / on_disconnected / on_message / on_error |

**接口方法**

| 分组 | 方法 | 返回 | 说明 |
|------|------|------|------|
| 生命周期 | `init()` | `bool` | 初始化网络通信 |
| | `shutdown()` | `void` | 关闭网络通信 |
| HTTP | `http_get(const std::string& url, ...)` | `HttpResponse` | 发送 GET 请求 |
| | `http_post(const std::string& url, const std::string& body, ...)` | `HttpResponse` | 发送 POST 请求 |
| WebSocket | `ws_connect(const std::string& url, WsCallback* callback)` | `bool` | 建立 WebSocket 连接 |
| | `ws_send(const WsMessage& message)` | `bool` | 发送 WebSocket 消息 |
| | `ws_close()` | `bool` | 关闭 WebSocket 连接 |

### 4.4 ConfigAdapter — JSON 配置文件读写

| 分组 | 方法 | 返回 | 说明 |
|------|------|------|------|
| 文件 | `load(const std::string& file_path)` | `bool` | 加载配置文件 |
| | `save()` | `bool` | 保存到当前文件 |
| | `save_as(const std::string& file_path)` | `bool` | 另存为指定路径 |
| 读取 | `get_string(const std::string& key)` | `std::optional<std::string>` | 读取字符串 |
| | `get_int(const std::string& key)` | `std::optional<int64_t>` | 读取整数 |
| | `get_double(const std::string& key)` | `std::optional<double>` | 读取浮点数 |
| | `get_bool(const std::string& key)` | `std::optional<bool>` | 读取布尔值 |
| 写入 | `set_string(const std::string& key, const std::string& value)` | `void` | 设置字符串 |
| | `set_int(const std::string& key, int64_t value)` | `void` | 设置整数 |
| | `set_double(const std::string& key, double value)` | `void` | 设置浮点数 |
| | `set_bool(const std::string& key, bool value)` | `void` | 设置布尔值 |
| 工具 | `has(const std::string& key)` | `bool` | 检查键是否存在 |

### 4.5 CryptoAdapter — 密码学操作

基于 OpenSSL 实现 SHA-256 哈希和 AES-256-GCM 加解密。

| 分组 | 方法 | 返回 | 说明 |
|------|------|------|------|
| 密码 | `hash_password(const std::string& password)` | `HashResult` | SHA-256 + 随机 Salt 哈希 |
| | `verify_password(const std::string& password, const std::string& hash, const std::string& salt)` | `bool` | 验证密码是否匹配 |
| 文件 | `encrypt_file(const std::string& input, const std::string& output, const std::string& key)` | `bool` | AES-256-GCM 加密文件 |
| | `decrypt_file(const std::string& input, const std::string& output, const std::string& key)` | `bool` | AES-256-GCM 解密文件 |
| 内存 | `encrypt_buffer(const uint8_t* data, size_t size, const std::string& key)` | `EncryptResult` | AES-256-GCM 加密内存数据 |
| | `decrypt_buffer(const uint8_t* data, size_t size, const std::string& key, const std::vector<uint8_t>& iv, const std::vector<uint8_t>& tag)` | `std::vector<uint8_t>` | AES-256-GCM 解密内存数据 |

**辅助类型**

| 类型 | 字段 | 说明 |
|------|------|------|
| `HashResult` | `.hash` `.salt` | 密码哈希结果（哈希值 + 随机盐） |
| `EncryptResult` | `.cipher_data` `.iv` `.tag` | AES 加密结果（密文 + IV + GCM 认证标签） |

---

## 5. Service 内部接口 — IServiceNetwork

> 命名空间：`Prism::Service`
> 头文件：`client/service/internal/IServiceNetwork.h`
> 隔离 Service 层与具体网络实现，当前为默认桩实现，后续替换为真实网络模块。

| 方法 | 返回 | 说明 |
|------|------|------|
| `login(const char* username, const char* password, const char* server_url)` | `int` | 登录服务器 |
| `logout()` | `int` | 登出 |
| `join_room(const char* room_id)` | `int` | 加入房间 |
| `leave_room()` | `int` | 离开房间 |
| `get_room_info(PrismRoomInfo* info)` | `int` | 获取房间信息 |
| `get_room_state()` | `PrismRoomState` | 获取房间连接状态 |

---

## 6. 接口索引

| 层级 | 命名空间 | 接口数 | 头文件目录 |
|------|----------|--------|------------|
| Service (C API) | `extern "C"` | 27 个函数 + 6 个结构体 + 5 个枚举 | `client/service/include/` |
| Service (内部) | `Prism::Service` | 1 个接口 | `client/service/internal/` |
| Business (AV Sync) | `Prism::Business` | 4 个接口 + 4 个工厂 | `client/business/av_sync/include/` |
| Business (Network) | `Prism::Business` | 3 个接口 + 1 个观察者 + 3 个工厂 | `client/business/network/include/` |
| Engine | `Prism::Engine` | 2 个接口 + 2 个工厂 + 3 个结构体 | `client/engine/include/` |
| Adapter | `Prism::Adapter` | 5 个接口 | `client/adapter/include/` |
