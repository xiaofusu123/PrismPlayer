# GLOSSARY

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
| Server | `服务端`、`prism-server`（小写）、`服务端程序` | 目标名是 `Prism-server` |

## CMake 目标与目录

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| `client-service` | `Prism-client`、`client` | **`Prism-client` 这个目标不存在**，`项目框架.md` 曾把它写成主目标，实际有 5 个 `client-*` + 1 个 `Prism-server` |
| `client-engine` | `engine`、`PrismEngine` | |
| `client-av_sync` | `client-avsync`、`client-av-sync`、`av_sync` 目标 | 下划线，与目录名一致 |
| `client-network` | `client-net`、`network` 目标 | |
| `client-adapter` | `client-adapters`、`adapter` 目标 | |
| `CMakeLists.txt` | **`CmakeLists.txt`**（小写 `m`） | 大小写敏感文件系统上 `add_subdirectory()` 找不到小写版本；Server 端目标平台是 Ubuntu，本机 Windows 永远测不出来。历史上 `server/` 与 `vendor/` 下 6 个文件是小写 |
| `windows-clang-x64-Debug` / `windows-clang-x64-Release` | `windows-x64-Debug`、`windows-Debug`、`windows-clang-x64-Releasee` | 预设全名带 `clang`；`Releasee` 是 `build.bat` 非法输入分支里的拼写错误 |
| `windows-base` | — | hidden 预设，上面两个继承它 |
| `test/` | **`tests/`** | 目录是单数。`代码规范.docx`（已归档）写的是 `tests`，以规范 §7 与实际目录为准 |
| `include/` + `internal/` + `src/` | `include/`+`src/`（两层）、`Impl/` | 每模块三层；历史上的 `Impl/` 目录已全部改名 `internal/` |

## AV Sync

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| `drift` | `偏移`、`偏差`、`offset`、`漂移量` | `drift = video_pts - audio_pts`，代码里的变量名就是它 |
| `ahead_threshold_ms` / `behind_threshold_ms` | `超前阈值`、`落后阈值`（不带 `_ms`） | 单位写进名字里；设计稿里的「阈值」在实现时必须是具体毫秒数 |
| `SyncAction`（`WAIT` / `DROP` / `RENDER`） | `SyncResult`、`Action` | 同步算法对当前帧的处置结论 |
| `SyncState` | `sync_state`、`SyncStatus`、`PlayerState` | AV Sync 模块的状态（在 `client/business/av_sync/include/SyncTypes.h`） |
| `sync_state`（7 态：`UNINIT`/`CALIBATING`/`SYNCHRONIZED`/`AHEAD`/`BEHIND`/`DISABLE`/`ERROR`） | — | ⚠️ **这是另一个东西**：它是 `practice/AudioDecoder.h` 里**音频解码器**的状态机，不是 AV Sync 的。两个不要混。且 `practice/` 未被 git 跟踪、不在构建中 |
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
| Network 的状态表述 | 「已实现」「未开始」「空模块」 | 统一写 **「占位（接口已定义，无实现）」**：`include/` 下 6 个头是真的，`internal/NetImpl.h` 与 `src/tmp.cpp` 是 0 字节 |
| `FileTransferManager`、`P2PConnectionManager` | — | ⚠️ **这两个组件在磁盘上不存在**，`项目框架.md` 曾把它们列为 Network 的 5 个组件之一。实际组件以 `include/` 下的 6 个头文件为准 |
| `ixwebsocket` | `websocketpp`、`IXWebSocket` | `websocketpp` 是 2026-06-01 冻结的 Word 旧版里的选型，已改 |

## Adapter

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| `FileAdapter`（接口） / `WinFileAdapter`（实现） | `FileSystemAdapter`、`FilesystemAdapter`、`FileImpl` | **`include/` 放抽象接口，`internal/` 放平台实现**，两者名字不同不是命名不一致 |
| `DBAdapter`（接口） / `SQLiteDBAdapter`（实现） | `DatabaseAdapter`、`SQLiteAdapter`、把接口也叫 `SQLiteDBAdapter` | 同上 |
| `CryptoAdapter`（接口） / `OpenSSLCryptoAdapter`（实现） | `SSLAdapter`、`OpenSSLAdapter`、把接口也叫 `OpenSSLCryptoAdapter` | 同上 |
| `ConfigAdapter`（接口） / `JSONConfigAdapter`（实现） | `SettingsAdapter`、`ConfigManager` | 同上 |
| `NetworkAdapter`（接口） / `ASIONetworkAdapter`（实现） | `SocketAdapter`、`NetAdapter` | 同上 |

> ⚠️ 说「适配器」时**指明是接口还是实现**：文档里列组件清单时用左边的接口名（这也是 `项目框架.md` 的做法，它没错），说具体实现时才用右边的名字。当前这两个名字的文件**都只有声明骨架**，`client/adapter/src/tmp.cpp` 是 0 字节。

## 版本、数值与标识

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| **32 个 C API** | 27 个 API、30 个接口 | `client/service/include/Player.h` 里 `_API` 出现 **32** 次，`Player.cpp` 里 32 个定义。`项目计划书.md` 与 `项目框架.md` 曾三处写「27 个」，是笔误 |
| `22.1.8` | `最新版`、`随便一个 clang-format` | 门禁工具版本钉死在此，见 `docs/adr/0002-代码门禁.md` |
| clang 工具链根目录 | `PATH 里的 clang` | 走**机器级**环境变量 `CLANG_ROOT`（默认 `D:\Program\App\msys64\clang64\bin`）、`VCPKG_ROOT`；这两个变量是 Machine 级、用户级为空 |
| `x64-mingw-dynamic` | `x64-windows`、`x64-windows-static` | 本项目的 vcpkg triplet |
| GitHub 登录名（`xiaofusu123`、`RickeyDeung`、`Rikka2-aa`、`ambulance001`、`kuailede110`） | **真名**（周炎杰、邓志鸿、梁兴邦、吴圹钛、辜垂沛） | 仓库是 **PUBLIC**。文档与 issue 中一律用 GitHub 登录名，真名不再出现在仓库里 |

## 成员 ↔ 模块（用于指派工单）

| GitHub 登录名 | 负责方向 |
|---|---|
| `xiaofusu123` | 文档与规范、门禁、Service 层、Server 端 |
| `RickeyDeung` | 见 issue 指派 |
| `Rikka2-aa` | 见 issue 指派 |
| `ambulance001` | Network 方向（历史上 3 次提交都在 `client/business/network/`） |
| `kuailede110` | 构建与基线方向（无历史提交） |

## 文档

| 用这个 | 不要写成 | 说明 |
|---|---|---|
| `docs/markdown/*.md` | `doc/`（单数）、`文档`、`md 文件` | **唯一真源**，见 `docs/adr/0003-文档真源.md`。目录 `doc/` 是旧名，已改名 |
| 六份文档的正名 | `设计说明`、`架构文档`、`API 文档`（带空格）、`接口文档` | `项目计划书.md`、`项目框架.md`、`代码规范.md`、`数据流.md`、`API接口文档.md`、`设计文档.md` |
| `docs/archive/` | `旧文档`、`docx 目录`、`docs/docx/` | 已过时，只读；`docs/docx/` 已不存在 |
| 已实现 / 仅骨架 / 仅设计 / 已废弃 | `完成`、`TODO`、`未完成` | 文档里描述实现状态时用这四个词（✅ / 🟡 / 📐 / ⛔），避免「基本完成」这类无法证伪的说法 |
