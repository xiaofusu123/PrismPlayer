# AGENTS.md

## 构建（前置条件与可复现边界）

干净 clone 到产物只需要两条命令（在仓库根目录执行）：

```powershell
pwsh -File scripts/build.ps1 1           # 1 = Debug，2 = Release
pwsh -File scripts/verify-build.ps1      # 断言产物存在且体积合理
```

```bash
bash scripts/build.sh 1
bash scripts/verify-build.sh
```

依赖安装树残缺、需要**显式清空后重装**时加 `--clean`：脚本先删掉该预设的整棵构建目录（含 `vcpkg_installed/` 的 manifest 与 ABI 戳，不删它们 vcpkg 会认为已装好而短路），再配置 + 构建。仓库内 `.vcpkg-cache`（已 gitignore）作为本机加速缓存保留。

### 要装什么

| 平台 | 工具链 | 依赖管理器 |
|------|--------|-----------|
| Windows | MSYS2 **mingw64** 环境的 clang / clang++ 23.1.3、ninja 1.13.2、GCC 16.2.0（vcpkg 用它编译依赖）；cmake 用独立安装的 4.3.2（`D:\Program\App\CMake\bin\cmake.exe`） | vcpkg checkout（`D:\Program\App\Vcpkg\vcpkg`） |
| WSL / Linux | clang / clang++ / clang-format / clang-tidy 23.1.2（Ubuntu 包）、cmake 4.4.4、ninja 1.11.1 | vcpkg checkout（Linux 可执行文件，`$HOME/vcpkg`） |

Windows 侧**必须用 mingw64 的 clang**，不能用已卸载的 clang64：vcpkg 的 `x64-mingw-dynamic` 依赖由同环境 GCC / libstdc++ 编译，clang64 的 clang 默认 libc++，跨标准库会让 `fmt` / `spdlog` 符号解析成两套 ABI，`libclient-service.dll` 链接失败（完整报错见 `docs/markdown/项目框架.md` 7.1）。

### 环境变量

| 变量 | Windows | WSL / Linux |
|------|---------|-------------|
| `CLANG_ROOT` | `D:\Program\App\msys64\mingw64\bin`（机器级） | 不使用（走 `/usr/bin/clang`） |
| `VCPKG_ROOT` | `D:\Program\App\Vcpkg\vcpkg`（机器级） | `$HOME/vcpkg`；非交互 shell 下由 `cmake/vcpkg-toolchain.cmake` 与 `scripts/wsl-env.sh` 自动探测 |

两个变量缺失时，配置阶段会给出可读报错并指出前置条件写在 `docs/markdown/项目框架.md` 7.1（由仓库内 `cmake/vcpkg-toolchain.cmake` 提供，不需要在预设里展开 `$env{VCPKG_ROOT}`）。

vcpkg baseline（`vcpkg-configuration.json` 的 `default-registry`）固定为 `e456309491fd875487bf02b8a0dca76c1b1b00cc`；重装后的完整依赖清单、文件数与实测产物体积见 `docs/markdown/项目框架.md` 7.1。

### 本阶段「可复现」的边界

到此为止：**同一工具链 + 同一 vcpkg baseline + 同一预设名 ⇒ 同一组产物**（4 个真实目标 `client-engine`、`client-av_sync`、`client-service`、`Prism-server`），并由 `scripts/verify-build.*` 断言存在与体积。

明确**不**保证：跨机器逐字节一致（路径、时间戳、缓存命中都会改变二进制）；容器化 / 隔离环境；`.vcpkg-cache`（仓库内、已 gitignore）在别的机器上可复用。

已实测的机器 / 环境：Windows（MSYS2 mingw64 + 本机 vcpkg checkout）与 WSL2 Ubuntu 24.04.5（同一台机器）。

## Agent skills

### Issue tracker

Issues and specs live as GitHub issues on `xiaofusu123/Prism-Player`, managed with the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Five canonical roles, label strings equal their names: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` at the repo root, ADRs under `docs/adr/`. See `docs/agents/domain.md`.
