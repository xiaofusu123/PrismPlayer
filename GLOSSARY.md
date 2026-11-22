# GLOSSARY

_术语与命名的唯一真源 · 2026-11-17_

---

**这里只收「同一个东西有多个叫法 / 多个写法」的词。** 普通概念（比如「解码器」「房间」）不进来 —— 词表一旦变成概念大全，就没人会查它。

用法：写文档、写 issue、写 commit、写代码注释时，**从左列的「用这个」里挑**；右列是历史上出现过的、应当废弃的说法。

## 层与模块

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| 表示层（UI） | `UI层`、`前端`、`view`、`client/UI/` | Flutter 侧，目录 `ui/` |
| FFI 桥接层（FFI） | `FFI层`、`桥接层` | `ui/lib/ffi/`，Dart ↔ C++ 的动态库加载与绑定 |
| 服务层（Service） | `Service层`、`门面层` | `client/service/`，对外 32 个 C API |
| 业务层（Business） | `Business层`、`service层`（小写 s，易与上一条混） | `client/business/`，含 AV Sync 与 Network 两个模块 |
| 引擎层（Engine） | `Engine层`、`解码层` | `client/engine/` |
| 适配器层（Adapter） | `Adapter层`、`基础设施层`、`adapter层` | `client/adapter/` |
| AV Sync | `av_sync`（说模块时）、`音视频同步`、`同步模块`、`sync` | 模块目录确实是 `av_sync`（下划线），但**说这个概念时写 AV Sync** |
| Network | `network`、`网络模块`、`net` | 模块目录是 `network` |
| Server | `服务端`、`prism-server`（小写）、`服务端程序` | 目标名是 `Prism-server`；实现 2026-11-23 移除，重写后恢复（ADR 0008） |

## CMake 目标与目录

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| `client-service` | `Prism-client`、`client` | **`Prism-client` 这个目标不存在**，`项目框架.md` 曾把它写成主目标，实际有 5 个 `client-*` 目标（`Prism-server` 随服务端实现于 2026-11-23 移除） |
| `client-engine` | `engine`、`PrismEngine` | |
| `client-av_sync` | `client-avsync`、`client-av-sync`、`av_sync` 目标 | 下划线，与目录名一致 |
| `client-network` | `client-net`、`network` 目标 | |
| `client-adapter` | `client-adapters`、`adapter` 目标 | |
| `CMakeLists.txt` | **`CmakeLists.txt`**（小写 `m`） | 大小写敏感文件系统上 `add_subdirectory()` 找不到小写版本。历史上 `server/` 与 `vendor/` 下共 6 个文件是小写：**`server/` 下 3 个已于 2026-10-11 改为大写**，`vendor/` 下剩余的仍是小写，**已决定暂不处理**（`vendor/` 不在根构建中；门禁的防回归检查必须对它例外） |
| `windows-clang-x64-Debug` / `windows-clang-x64-Release` / `linux-clang-x64-Debug` / `linux-clang-x64-Release` | `windows-x64-Debug`、`windows-Debug`、`windows-clang-x64-Releasee` | 预设全名带平台前缀与 `clang`；现共 **6 配置 + 6 构建 + 6 test 预设**（Debug / Release / Coverage × Windows / Linux），`binaryDir`（`build/{Windows,Linux}/{debug,release,coverage}`）；`testPresets` 于 2026-11-02 由 #54 新增，`ctest --preset windows-clang-x64-Debug` 可直接运行。后两个是历史名字：`Releasee` 是旧 `build.bat` 非法输入分支里的拼写错误，**已于 2026-10-11 修正，不再是当前仓库的写法** |
| `windows-base` | — | hidden 预设，Windows 侧具体预设继承它 |
| `test/` | **`tests/`** | 目录是单数。`代码规范.docx`（已随 `docs/archive/` 删除）写的是 `tests`，以规范 §7 与实际目录为准 |
| `include/` + `internal/` + `src/` | `include/`+`src/`（两层）、`Impl/` | 每模块三层；历史上的 `Impl/` 目录已全部改名 `internal/` |

## AV Sync

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| `drift` | `偏移`、`偏差`、`offset`、`漂移量` | `drift = video_pts - audio_pts`，代码里的变量名就是它 |
| `ahead_threshold_ms` / `behind_threshold_ms` | `超前阈值`、`落后阈值`（不带 `_ms`） | 单位写进名字里；设计稿里的「阈值」在实现时必须是具体毫秒数 |
| `SyncAction`（`WAIT` / `DROP` / `RENDER`） | `SyncResult`、`Action` | 同步算法对当前帧的处置结论 |
| `SyncState` | `sync_state`、`SyncStatus`、`PlayerState` | AV Sync 模块的状态（在 `client/business/av_sync/include/SyncTypes.h`） |
| `SyncState` 的 7 个取值 | `UNINIT`/`CALIBATING`/`SYNCHRONIZED`/`AHEAD`/`BEHIND`/`DISABLE`/**`SYNC_ERROR`** | AV Sync 的状态枚举在 `client/business/av_sync/include/SyncTypes.h`；注意最后一态叫 `SYNC_ERROR`（早期设计稿 `practice/AudioDecoder.h` 里同名状态机写成 `ERROR`，那份文件未被 git 跟踪、不在构建中，以 `SyncTypes.h` 为准） |
| `EngineObserver` | `Observer`、`EngineListener` | |
| `PlaybackStateMachine` | `StateMachine`、`PlayerFSM` | |
| `CommandDispatcher` | `Dispatcher`、`CommandHandler` | 唯一没有旧测试覆盖的 AV Sync 组件 |

## 网络

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| `IAccountManager` | `AccountManager`、`UserManager` | 接口名带 `I` 前缀（这是本仓库的既有惯例，规范 §1.1 允许） |
| `IRoomManager` | `RoomManager` | |
| `ISignalingClient` | `SignalingClient`、`SignalClient`、`WebSocketClient` | |
| `INetworkObserver` | `NetworkObserver`、`NetworkCallback` | |
| `NetworkFactory` | `NetworkClientFactory` | |
| `NetworkTypes` | `NetworkDefs`、`Types` | |
| Network 的状态表述 | 「已实现」「未开始」「空模块」 | 统一写 **「占位（接口已定义，无实现）」**：`include/` 下 6 个头是真的，`internal/NetImpl.h` 是 0 字节（`src/tmp.cpp` 这个占位文件已于 2026-10-11 删除，`client-network` 目标不再生成） |
| `FileTransferManager`、`P2PConnectionManager` | — | ⚠️ **这两个组件在磁盘上不存在**，`项目框架.md` 曾把它们列为 Network 的 5 个组件之一。实际组件以 `include/` 下的 6 个头文件为准 |
| `ixwebsocket` | `websocketpp`、`IXWebSocket` | `websocketpp` 是 2026-06-01 冻结的 Word 旧版里的选型，已改 |

## Adapter

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| `FileAdapter`（接口） / `WinFileAdapter`（实现） | `FileSystemAdapter`、`FilesystemAdapter`、`FileImpl` | **`include/` 放抽象接口，`internal/` 放平台实现**，两者名字不同不是命名不一致 |
| `DBAdapter`（接口） / `SQLiteDBAdapter`（实现） | `DatabaseAdapter`、`SQLiteAdapter`、把接口也叫 `SQLiteDBAdapter` | 同上 |
| `CryptoAdapter`（接口） / `OpenSSLCryptoAdapter`（实现） | `SSLAdapter`、`OpenSSLAdapter`、把接口也叫 `OpenSSLCryptoAdapter` | 同上 |
| `ConfigAdapter`（接口） / `JSONConfigAdapter`（实现） | `SettingsAdapter`、`ConfigManager` | 同上 |
| `NetworkAdapter`（接口） / `ASIONetworkAdapter`（实现） | `SocketAdapter`、`NetAdapter` | 同上；⚠️ 类名带 ASIO，但实现里用的是 `ixwebsocket`（全仓无一处 `#include <asio…>`），别按名字推断实现路线 |
| `FileCipher`（接口） / `OpenSSLFileCipher`（实现） | 把文件加密塞进 `CryptoAdapter`、`FileEncryptor` | **文件级分块 AEAD 不在 `CryptoAdapter` 里**（ADR 0007 决策 6）：`CryptoAdapter` 只管口令哈希与 buffer 加密，文件加密归 `FileCipher`，尚未实现（#166） |
| `credential`（自描述串） | `password_hash`（当跨侧契约说时）、`hash` + `salt` 分开存 | 形状 `sha256$<盐 hex>$<摘要 hex>`，客户端与服务端**都存这一个串**，`verify_password(password, credential)` 两参数（#117 决策 23/44） |

> ⚠️ 说「适配器」时**指明是接口还是实现**：文档里列组件清单时用左边的接口名（这也是 `项目框架.md` 的做法，它没错），说具体实现时才用右边的名字。`FileAdapter` → `WinFileAdapter` 与 `ConfigAdapter` → `JSONConfigAdapter` **已有实现**（分别在 `src/WinFileAdapter.cpp`、`src/JSONConfigAdapter.cpp`），五个实现**全部已落地**（#159）：`SQLiteDBAdapter` 608 行、`OpenSSLCryptoAdapter` 375 行、`ASIONetworkAdapter` 313 行，`client-adapter` 产出非空静态库（5 个 cpp 共 1,706 行）。

## 版本、数值与标识

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| **32 个 C API** | 27 个 API、30 个接口 | `client/service/include/Player.h` 里 `PRISM_API` 出现 **32** 次，`Player.cpp` 里 32 个定义。`项目计划书.md` 与 `项目框架.md` 曾三处写「27 个」，是笔误 |
| `23.1`（补丁位随安装更新） | `最新版`、`随便一个 clang-format`、写死 `23.1.3` | 门禁只比对前缀 `23.1`；2026-11-17 实测 Windows `23.1.4`、WSL `23.1.5`（2026-10-12 起从 clang64 迁到 mingw64 时是 `23.1.3`，此前为 `22.1.8`）。见 `docs/adr/0002-代码门禁.md` 与 `AGENTS.md` 的「构建」一节 |
| clang 工具链根目录 | `PATH 里的 clang` | 走**机器级**环境变量 `CLANG_ROOT`（当前 **`D:\Program\Dev\msys64\mingw64\bin`**；2026-10-12 起，此前为 `clang64\bin`）、`VCPKG_ROOT`（`D:\Program\Dev\Vcpkg\vcpkg`）；这两个变量是 Machine 级、用户级为空 |
| `x64-mingw-dynamic` | `x64-windows`、`x64-windows-static` | 本项目的 vcpkg triplet |
| GitHub 登录名（`xiaofusu123`、`RickeyDeung`、`Rikka2-aa`、`ambulance001`、`kuailede110`） | 成员真名；旧提交里的作者显示名 `xiaofusu`、`QuaSar`、`Rikka--aa`、`天才奇男子` | 仓库是 **PUBLIC**：文档、代码注释与 issue 里**一律**用 GitHub 登录名。`git log` 里看到的 `xiaofusu` / `QuaSar` / `Rikka--aa` / `天才奇男子` 是历史提交的作者显示名，**不是**登录名，也不代表成员还叫这个名字；本文档不再收录真名 |

## 成员 ↔ 模块（用于指派工单）

| GitHub 登录名 | 负责方向 |
|---|---|
| `xiaofusu123` | 文档与规范、门禁、构建与基线、Service 层、Server 端、Engine 层 |
| `RickeyDeung` | 见 issue 指派 |
| `Rikka2-aa` | 数据库设计、Adapter 层（历史上 2 次提交都在 `client/adapter/`） |
| `ambulance001` | Network 方向（历史上 3 次提交都在 `client/business/network/`） |
| `kuailede110` | UI 层 (Flutter) 方向（无历史提交；已退出本仓库协作，分工不变） |

## 文档

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| `docs/markdown/*.md` | `doc/`（单数）、`文档`、`md 文件` | **唯一真源**，见 `docs/adr/0003-文档真源.md`。目录 `doc/` 是旧名，已改名 |
| 六份文档的正名 | `设计说明`、`架构文档`、`API 文档`（带空格）、`接口文档` | `项目计划书.md`、`项目框架.md`、`代码规范.md`、`数据流.md`、`API接口文档.md`、`设计文档.md` |
| `docs/archive/` | `旧文档`、`docx 目录`、`docs/docx/` | 已于 2026-11-02 删除（2 份 Word 旧版内容已全量并入 md）；`docs/docx/` 更早改名后不存在 |
| 已实现 / 仅骨架 / 仅设计 / 已废弃 | `完成`、`TODO`、`未完成` | 文档里描述实现状态时用这四个词（✅ / 🟡 / 📐 / ⛔），避免「基本完成」这类无法证伪的说法 |
| 每份文档的单一职责 | 同一张表在 3~6 份文档里各抄一遍 | **想知道什么 → 看哪份**：`项目计划书.md` 管「要做什么、什么时候做、依赖与数据库真源、当前状态」；`项目框架.md` 管「代码长什么样、文件在哪」；`设计文档.md` 管「为什么这么设计、选型理由」；`数据流.md` 管「一次播放/一次登录的数据怎么走」；`API接口文档.md` 管「公开接口签名与语义」；`代码规范.md` 管「怎么写、怎么合」 |
