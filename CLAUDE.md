# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires `VCPKG_ROOT` and `CLANG_ROOT` environment variables:
- `VCPKG_ROOT` — vcpkg installation root (used for toolchain file)
- `CLANG_ROOT` — LLVM installation root (must also contain `ninja.exe`; used via `$env{CLANG_ROOT}/ninja.exe` in presets)

- **Configure + Build (Windows)**: `build.bat` (interactive, select preset) or direct:
  - `cmake --preset windows-x64-Debug` then `cmake --build --preset windows-x64-Debug`
  - `cmake --preset windows-x64-Release` then `cmake --build --preset windows-x64-Release`
- **build.sh** is a stub — only echoes paths, no build commands.
- **Tests**: gtest via vcpkg; tests live in `test/`. `enable_testing()` is only called in Debug builds. The test executable in the root CMakeLists.txt is currently commented out — no test target exists yet.
- **Compile definitions**: `_DEBUG` set for Debug builds, `_RELEASE` for Release builds.

## Architecture

PrismPlayer is a C++17 media player with client-server architecture, built with CMake + Ninja + Clang + vcpkg.

### Top-level split

- `client/` — Prism-client shared library (player engine, networking, UI). The DLL export macro `_API` (defined in `client/common/API.h`) uses `__declspec(dllexport)` when `LAYERING_EXPORTS` is set. **Bug**: the client CMakeLists.txt defines `_PRISM_EXPORTS` instead of `LAYERING_EXPORTS`, so the export mechanism is currently broken. Non-Windows hides symbols by default.
- `server/` — Prism-server executable (`server/main.cpp`). Links adapter, network, and business libraries.

### Client module dependency chain (bottom-up)

```
adapter  (base interfaces/abstractions)
  ↑
engine   (ffmpeg decoding, Vulkan rendering, glfw3 windowing, soundtouch audio)
  ↑
business/av_sync   (A/V sync logic)
business/network   (asio + ixwebsocket, depends on client-adapter)
  ↑
service  (orchestration layer, depends on av_sync + network)
```

`client/common/` holds shared headers included globally: `API.h` defines the `_API` export/import macro; `pch.h` is currently an empty placeholder.

### Server modules

```
adapter → network (asio, nlohmann-json) → business
```

Server `business/` links adapter + network; the executable links all three.

### Per-module directory convention

Each module (under both client and server) follows:

```
module/
  include/     public headers (API)
  Impl/        private/implementation headers
  src/         .cpp implementation files
```

The build uses `aux_source_directory(src/)` to collect sources, so new `.cpp` files in `src/` are picked up automatically. New modules must be added to the parent `CmakeLists.txt` via `add_subdirectory()`.

### Key dependencies (vcpkg)

| Library | Used by | Purpose |
|---------|---------|---------|
| ffmpeg | client-engine | Audio/video decode/encode |
| vulkan, glfw3 | client-engine | Rendering and windowing |
| soundtouch | client-engine | Audio time-stretching/pitch-shifting |
| asio | client-network, server-network | Async networking |
| ixwebsocket | client-network | WebSocket client |
| nlohmann-json | client-network, server-network | JSON serialization |
| glm | (planned, av_sync) | Vector math |
| gtest | test/ | Testing framework |
| openssl | (dependency) | TLS/crypto |
| spdlog | (dependency) | Structured logging |

### Vendor libraries

`vendor/` contains local C++ libraries (not fetched by vcpkg), each with its own standalone CMakeLists.txt. They are **not integrated into the main build** — they must be built independently.
- `Memory_Pool/` — custom memory pool allocator
- `Thread_Pool/` — custom thread pool

### Namespace convention

Public API uses `Prism::core`. The only real (non-placeholder) header following this convention is `Audio_Decoder.h`.

### Project documentation

`docs/` contains design documents, coding standards, and project plan (all in Chinese, `.docx` format).

### Current state

The project is early-stage architecture scaffolding. Almost all `src/` and `Impl/` directories contain only `tmp.*` placeholder files. The only real API defined is `Audio_Decoder.h` (pure virtual interface in `Prism::core`). `server/main.cpp` is empty. No test targets are wired up. The UI layer (`client/view/`) is planned for Flutter/Dart.
