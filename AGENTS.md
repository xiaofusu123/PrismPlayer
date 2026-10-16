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

## 代码门禁（本地 Git hooks + 服务端 CI，#64 / #96）

提交与推送由仓库自带的钩子把关（**快反馈层**：几秒到几十秒），服务端还有一层**不可绕过层**（GitHub Actions，见下「服务端检查」）。钩子每个 clone / 每台机器装一次：

```powershell
pwsh -File scripts/install-hooks.ps1
```

```bash
bash scripts/install-hooks.sh
```

脚本给 `.githooks/*` 加执行位、把 `core.hooksPath` 指向 `.githooks`，再读回校验；`git config --local --get core.hooksPath` 应输出 `.githooks`。钩子统一 `#!/bin/sh`，由 Git for Windows 自带的 sh 执行，不依赖 PowerShell / cmd / Python。

### 查什么

| 钩子 | 检查 | 判定对象 |
|------|------|---------|
| `pre-commit` | staged 的 `.c/.cc/.cpp/.cxx/.h/.hh/.hpp/.hxx` 必须过 `clang-format --dry-run --Werror`；文件名不得是 `CmakeLists.txt` | **index 内容**（`git show :<文件>`）：未 staged 的文件不查，工作区改坏不影响 |
| `pre-push` | 先跑 `<平台>-clang-x64-Debug` 构建（只 Debug，不跑 ctest / Release），成功后对**本次推送相对默认分支改动**的 C/C++ 文件跑 `.clang-tidy` | 构建失败即拒绝；静态检查只查改动文件，加沿 include 链 3 层内的编译单元，覆盖不到的文件会明确列出 |

`vendor/**` 不参与构建，只做格式检查，不受 `CmakeLists.txt` 重命名检查约束。

三条 clang-tidy 规则与规范的对应见 `docs/markdown/代码规范.md` §0（命名 §1.1–§1.3、虚基类析构 §4.6、函数 ≤100 行 §5.1）；为什么只启这三条、为什么 clang-tidy 放在 pre-push，见 `docs/adr/0002-代码门禁.md`。规则不是打开就完事：**先加检查 → 先清零基线 → 再设成致命错误**，`--no-verify` 只在例外场景用。

### 版本要求

钩子第一件事比对工具版本前缀，必须是 **`23.1`**（Windows 23.1.3、WSL / Linux 23.1.2），不符合或找不到就立刻退出并说明原因。编译数据库由两个基预设的 `CMAKE_EXPORT_COMPILE_COMMANDS=ON` 产出；缺失时钩子自己跑一次配置。

### 实测耗时（#70）

预算 900 秒（`PRISM_TIME_BUDGET_SECONDS`，超了只提示、不拒绝）。2026-10-13 在 Windows + mingw64 clang 23.1.3 实测：

| 场景 | 耗时 |
|------|------|
| 冷配置（`build/Windows/debug` 从零，依赖走 `.vcpkg-cache`） | 1,071 秒 —— 只在构建树缺失、钩子自愈时发生 |
| 热构建 + 改动文件的 clang-tidy（4 个文件、6 个编译单元） | 24–29 秒 |
| 没有 C/C++ 改动 | clang-tidy 直接跳过（连编译数据库都不要求），只剩构建增量；`ninja: no work to do.` 时 1 秒内 |
| 全仓 clang-tidy（76 个文件、11 个编译单元） | 43 秒 |

构建失败时钩子以非零码退出并明确说是「代码编译不过，不是钩子故障」，附日志路径与本地复现命令；前置条件不满足（缺 `CLANG_ROOT` / `VCPKG_ROOT` / cmake）时给可读提示并指向 `docs/markdown/项目框架.md` 7.1，不倒 CMake 原始报错。

### 未整理的文件与唯一允许的绕过

仓库里多数 C/C++ 文件还没跑过 `.clang-format`（#68 明确不做全仓一次性重排）。**碰到哪个文件就整理哪个**：staged 的文件必须合规，报错里直接给可复制的修复命令。确需一次性批量整理时，`git commit --no-verify` / `git push --no-verify` 是唯一允许的绕过，且必须在 PR 描述里说明。

自测：`sh test/hooks/run.sh`（临时仓库里跑真实钩子，断言退出码与 stderr）与 `sh test/checks/run.sh`（检查入口的 CLI 契约、范围语义、以及「不 fail-open」）。

### 服务端检查（GitHub Actions，#96）

本地钩子会被 `--no-verify` 绕过，也可能因为「这台机器根本没装钩子」而压根不生效，所以 `main` 上另有一层跑在 GitHub 上的检查：`.github/workflows/gate.yml`。**判定逻辑不写在 YAML 里** —— workflow 只负责装工具链、还原缓存、按预设配置与构建，判定一律调用 `scripts/checks/` 与 `test/` 下的**同一份入口**（本地钩子调的也是它），所以「CI 说绿、本机说红」在结构上不可能出现。

| job | 查什么 | 判定入口 |
|-----|--------|---------|
| `hooks-self-test` | 钩子自身在干净机器上成立 | `sh test/hooks/run.sh` |
| `checks-self-test` | 检查入口的 CLI 契约与范围语义 | `sh test/checks/run.sh` |
| `format` | **本次变更范围**内 C/C++ 文件的格式（判定对象等价于 staged 内容） | `sh scripts/checks/format.sh ci` |
| `build-linux-debug` | Ubuntu 24.04 + `linux-clang-x64-Debug` 从零配置 + 构建 + 产物契约（体积阈值、≥32 个 `prism_player_*` 导出、不含 libc++） | `scripts/ci/preset-linux-debug.sh configure` / `build` / `verify` |
| `tidy` | 本次变更范围内的 clang-tidy（与 `pre-push` 同一份判定） | `sh scripts/checks/tidy.sh build/Linux/debug ci` |
| `ctest` | 有没有测试可跑；未注册用例时明确 SKIP，不假装绿 | `sh scripts/checks/ctest.sh build/Linux/debug` |

上表六个 job（`hooks-self-test`、`checks-self-test`、`format`、`build-linux-debug`、`tidy`、`ctest`）**全部**是 `main` 规则集里的**必需状态检查**；`cache-key` 只是内部步骤，不是契约的一部分（改名、增删 job 都必须同步规则集，理由见下）。

本地复现同一个结论（WSL / Linux，仓库根）：

```bash
bash scripts/ci/setup-linux.sh llvm cmake system vcpkg
bash scripts/ci/preset-linux-debug.sh configure && bash scripts/ci/preset-linux-debug.sh build
bash scripts/ci/preset-linux-debug.sh verify
sh scripts/checks/format.sh ci
sh scripts/checks/tidy.sh build/Linux/debug ci
sh scripts/checks/ctest.sh build/Linux/debug
```

**CI 只跑 Linux Debug**：Windows 与 Release 由本机钩子覆盖（Server 的目标平台就是 Ubuntu，CI 对它最有说服力）。workflow 把 `CLANG_ROOT` 设成 `/usr/lib/llvm-23/bin`、`VCPKG_ROOT` 设成 `/home/runner/vcpkg`；工具版本前缀仍然必须是 `23.1`，不符即失败退出（判定复用 `scripts/checks/lib.sh`，与本机同一份）。

**job 名是契约**：`main` 的规则集（id `15828446`）把这些 job 名设成**必需状态检查**，所以**改名必须同步规则集** —— 否则必需检查会静默失效（GitHub 不会因为「一个从没出现过的检查」而拦住谁，它只会一直等下去）。实测（#103）：必需检查会让「有红检查的 PR」用普通方式合不动，报 `the base branch policy prohibits the merge`；同时本仓库的规则集还要求 1 条 review，单人仓库只能自审，所以 `--admin` 仍能合并 —— 但每一次绕过都在 PR 历史上留下记录（见 `docs/adr/0006-门禁第二阶段CI.md` 决策 6）。

**依赖与缓存**：冷启动要在干净 runner 上从源码编译 ffmpeg / glfw3 / vulkan 等依赖（十几分钟量级），命中 vcpkg 二进制缓存后落到几分钟量级。缓存键 = vcpkg baseline + `vcpkg.json` 内容哈希 + triplet + runner 镜像 + clang 前缀，在 `cache-key` job 里只算一次并打进日志；**未命中不是故障**，而是「有东西变了」—— 把日志里那行键与上一次绿灯的逐段比对就知道是哪一个。冷热实测耗时、缓存路径与「CI 变慢通常意味着什么」见 `docs/adr/0006-门禁第二阶段CI.md`。

## Agent skills

### Issue tracker

Issues and specs live as GitHub issues on `xiaofusu123/Prism-Player`, managed with the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Five canonical roles, label strings equal their names: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` at the repo root, ADRs under `docs/adr/`. See `docs/agents/domain.md`.
