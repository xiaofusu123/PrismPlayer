# PrismPlayer API Design Specification

**Author:** 邓志鸿  
**Date:** 2026-05-27  
**Scope:** Engine Layer, Business Layer (AV Sync), Service Layer (C API)  
**Project:** PrismPlayer — cross-platform audio/video media player

## Architecture Overview

Six-layer client architecture per project plan. This spec covers layers 3-5:

```
Presentation (Flutter/Dart, 赖艺邦)
    ↑ FFI (C ABI)
Service (unified C API)          ← this spec
    ↑ C++ classes
Business (AV Sync, Network)      ← this spec (AV Sync only)
    ↑ interfaces
Engine (Codec, Rendering, Audio) ← this spec
    ↑
Adapter (DB, File I/O)           ← 彭嘉豪
```

**Design principle:** Layered Abstraction. Engine = pure virtual interfaces (platform-swappable). Business = concrete classes. Service = opaque C handles wrapping C++ facade. Matches existing `Audio_Decoder.h` pattern.

## Naming Conventions (from 代码规范.docx)

| Category | Style | Example |
|----------|-------|---------|
| Struct/Class/Enum | PascalCase | `AudioDecoder`, `PlayerState` |
| Function/Variable | snake_case | `init()`, `sample_rate` |
| Macro/Constant | UPPER_SNAKE | `MAX_BUFFER_SIZE` |
| Struct members | snake_case, default-initialized to 0 | `int bit_rate = 0;` |
| Namespace | PascalCase | `Prism::core` |
| Headers | .h | `Audio_Decoder.h` |
| Sources | .cpp | `audio_decoder_impl.cpp` |

## Module Directory Convention (existing project standard)

```
module/
  include/     public headers
  Impl/        private implementation headers
  src/         .cpp files (auto-collected via aux_source_directory)
```

---

## Part 1: Engine Layer (接口层)

### 1.1 AudioDecoder — Refine existing `Audio_Decoder.h`

**Keep existing interface** with minor cleanup. Already follows coding standards.

```
File: client/engine/include/Audio_Decoder.h  (existing, keep)
Namespace: Prism::core

class Audio_Decoder (pure virtual):
  + init(const audio_decoder_config&) → bool
  + sync_decode() → bool
  + async_decode() → bool
  + restart() → bool
  + stop() → bool
  + flush() → bool
  + reset() → bool
  + free() → bool
  + isRunning() → bool
  + get_config_info() const → audio_decoder_config
  + get_status_info() const → audio_decoder_config

struct audio_decoder_config (existing, keep):
  codec_id, bit_rate, sample_rate, channels,
  channel_layout, sample_fmt — all = 0 or AV defaults
```

### 1.2 VideoDecoder — New, mirrors AudioDecoder lifecycle

```
File: client/engine/include/Video_Decoder.h  (new)
Namespace: Prism::core

struct video_decoder_config:
  AVCodecID codec_id = AV_CODEC_ID_NONE;
  int width = 0;
  int height = 0;
  AVPixelFormat pixel_fmt = AV_PIX_FMT_NONE;
  AVRational frame_rate = {0, 1};
  int bit_rate = 0;
  // all members default-initialized

class Video_Decoder (pure virtual):
  + init(const video_decoder_config&) → bool
  + sync_decode() → bool
  + async_decode() → bool
  + restart() → bool
  + stop() → bool
  + flush() → bool
  + reset() → bool
  + free() → bool
  + isRunning() → bool
  + get_config_info() const → video_decoder_config
  + get_status_info() const → video_decoder_config
```

### 1.3 Renderer — Vulkan rendering + glfw3 windowing

```
File: client/engine/include/Renderer.h  (new)
Namespace: Prism::core

struct renderer_config:
  void* native_window_handle = nullptr;
  int surface_width = 0;
  int surface_height = 0;
  bool vsync = true;
  uint8_t backbuffer_count = 2;

class Renderer (pure virtual):
  + init(const renderer_config&) → bool
  + render_video_frame(const void* frame_data) → bool
  + swap_buffers()
  + set_video_region(int x, int y, int w, int h)
  + resize(int width, int height)
  + stop() → bool
  + free() → bool
  + isRunning() → bool
```

### 1.4 AudioPlayer — PCM playback + SoundTouch

```
File: client/engine/include/Audio_Player.h  (new)
Namespace: Prism::core

struct audio_player_config:
  int sample_rate = 44100;
  int channels = 2;
  AVSampleFormat sample_fmt = AV_SAMPLE_FMT_S16;
  int buffer_size_ms = 50;
  float default_volume = 1.0f;

class Audio_Player (pure virtual):
  + init(const audio_player_config&) → bool
  + push_samples(const void* data, size_t count) → bool
  + pause()
  + resume()
  + flush()
  + set_volume(float level)       // 0.0 - 1.0
  + set_playback_speed(float rate)   // SoundTouch
  + get_queued_duration_ms() const → int64_t
  + free() → bool
  + isRunning() → bool
```

---

## Part 2: Business Layer — AV Sync (业务层)

### 2.1 AVSyncController — Concrete class

```
File: client/business/av_sync/include/AV_Sync_Controller.h  (new)
Namespace: Prism::core

enum class SyncMaster:
  AUDIO,
  VIDEO,
  EXTERNAL

struct av_sync_config:
  SyncMaster master = SyncMaster::AUDIO;
  int64_t sync_threshold_ms = 50;     // max drift before correction
  int64_t min_adjustment_ms = 5;      // smallest correction to apply
  double max_playback_speed = 2.0;    // SoundTouch upper limit
  double min_playback_speed = 0.5;    // SoundTouch lower limit

class AV_Sync_Controller (concrete):
  + init(av_sync_config) → bool
  + set_audio_clock(int64_t pts_ms)     // from audio decoder
  + set_video_clock(int64_t pts_ms)     // from video decoder
  + get_sync_adjustment_ms() const → int64_t   // +/- delay for video
  + get_master_clock_pts() const → int64_t
  + get_audio_clock_drift_ms() const → int64_t  // for monitoring
  + set_master(SyncMaster)
  + set_sync_threshold(int64_t ms)
  + reset()
```

**Data flow:** Decoders feed clock pts → AVSyncController computes drift → video renderer adjusts frame timing via `get_sync_adjustment_ms()`. Audio is the default master because human ears are more sensitive to timing changes than eyes.

---

## Part 3: Service Layer — C API (服务层)

### 3.1 Design rationale

Pure C API with opaque handles. Flutter's `dart:ffi` calls these directly. The C++ implementation (service/src/) wraps the engine and business layer objects behind the handle.

### 3.2 Types (`prism_types.h`)

```
File: client/service/include/prism_types.h  (new)

typedef struct PrismPlayerInternal* PrismPlayerHandle;

typedef enum {
    PRISM_STATE_IDLE = 0,
    PRISM_STATE_LOADING,
    PRISM_STATE_PLAYING,
    PRISM_STATE_PAUSED,
    PRISM_STATE_STOPPED,
    PRISM_STATE_ERROR
} PrismState;

typedef enum {
    PRISM_EVENT_MEDIA_LOADED = 0,
    PRISM_EVENT_PLAYBACK_COMPLETED,
    PRISM_EVENT_SEEK_COMPLETED,
    PRISM_EVENT_BUFFERING_START,
    PRISM_EVENT_BUFFERING_END,
    PRISM_EVENT_ERROR
} PrismEventType;

typedef void (*PrismEventCallback)(PrismEventType, const void* data, void* user_data);

typedef struct {
    int video_output_width;        // 0 = source native
    int video_output_height;       // 0 = source native
    int audio_sample_rate;         // 0 = source native
    float default_volume;          // 0.0-1.0, default 1.0
    bool enable_video;             // false = audio-only mode
    const char* log_level;         // "debug"|"info"|"warn"|"error"
} PrismConfig;
```

### 3.3 Player API (`prism_player.h`)

```
File: client/service/include/prism_player.h  (new)

// Lifecycle
PrismPlayerHandle* prism_player_create(const PrismConfig* config);
void prism_player_destroy(PrismPlayerHandle* handle);
PrismState prism_player_get_state(const PrismPlayerHandle* handle);

// Playback control
bool prism_player_play(PrismPlayerHandle* handle);
bool prism_player_pause(PrismPlayerHandle* handle);
bool prism_player_stop(PrismPlayerHandle* handle);
bool prism_player_seek(PrismPlayerHandle* handle, int64_t position_ms);
int64_t prism_player_get_position(const PrismPlayerHandle* handle);
int64_t prism_player_get_duration(const PrismPlayerHandle* handle);
bool prism_player_set_volume(PrismPlayerHandle* handle, float level);
bool prism_player_set_playback_speed(PrismPlayerHandle* handle, float rate);

// Media source
bool prism_player_open_uri(PrismPlayerHandle* handle, const char* uri);
bool prism_player_close_media(PrismPlayerHandle* handle);

// Event callback
void prism_player_set_event_callback(PrismPlayerHandle* handle,
                                      PrismEventCallback callback,
                                      void* user_data);

// Error handling
int prism_player_get_error(const PrismPlayerHandle* handle,
                           char* buffer, int buffer_size);
void prism_player_clear_error(PrismPlayerHandle* handle);
```

All functions returning `bool`: `true` = success, `false` = error (call `prism_player_get_error()` for details).

### 3.4 Service layer implementation structure

```
client/service/
  include/
    prism_player.h       C API header
    prism_types.h        C types/enums
  Impl/
    player_impl.h        C++ Player class wrapping engine + business
  src/
    prism_player.cpp     C API implementation (extern "C")
    player_impl.cpp      Player class implementation
```

`PrismPlayerInternal` is defined as a C++ class in `player_impl.h`, containing owned instances of all engine and business layer components, wired by the service layer.

---

## Error Handling Strategy

- Engine interfaces: `bool` return values, spdlog for diagnostics
- AVSyncController: `bool` return values, spdlog for diagnostics
- Service C API: `bool` return + `prism_player_get_error()` for string details
- All struct members default-initialized to 0/nullptr/null so partial initialization is safe

## Logging (spdlog)

Per coding standards, log at key decision points: init success/failure, decode start/end, sync adjustments exceeding threshold, state transitions, and errors.

## Testing Strategy

- Unit tests per module in `test/`, named `<module>_test.cpp`, gtest framework
- Target: ≥80% line coverage per coding standards
- Engine tests: mock configs, feed known frames/samples, verify output
- AVSync tests: inject clock values, verify sync adjustment calculations
- Service tests: create/destroy lifecycle, state transitions, error paths

## What This Spec Does NOT Cover

- Network module (邱富演's responsibility)
- Adapter/data persistence layer (彭嘉豪's responsibility)
- Flutter/Presentation layer (赖艺邦's responsibility)
- Actual decoder/renderer implementations (header-only interfaces for now)
- Player_Impl wiring details (depends on engine Impl availability)
