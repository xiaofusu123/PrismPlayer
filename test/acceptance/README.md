# 验收目标（test-acceptance）

`test/acceptance/` 是 #88 交付的**验收骨架**：只通过 32 个公开 C ABI（`client/service/include/Player.h`）
驱动播放器，用记录型假音频/视频引擎顶替真实设备，产出可机器检查的证据报告。

它不 include 任何 `internal/` 头。假引擎通过既有 ABI
`prism_player_set_audio_factory` / `prism_player_set_video_factory` 注入，C ABI 不变。

## 怎么跑

先构建 Debug（测试目标只在 `CMAKE_BUILD_TYPE=Debug` 下生成）：

```powershell
pwsh -File scripts/build/build.ps1 1
pwsh -File scripts/e2e/run-acceptance.ps1
```

```bash
bash scripts/build/build.sh 1
bash scripts/e2e/run-acceptance.sh
```

两个编排脚本在**参数、默认值、退出码**上一致：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--preset <name>` | `windows-clang-x64-Debug` / `linux-clang-x64-Debug` | 构建预设；由它推出 `build/<平台>/<配置>` |
| `--mode a\|b` | `a` | `a` = in-process；`b` = multi-process（当前只记录“未交付”的 skip） |
| `--perf` | 关 | 请求性能用例（当前只记录“未交付”的 skip） |
| `--coverage-report <path>` | `build/<平台>/coverage/coverage-report.txt` | #84 的覆盖率报告；相对路径按仓库根解析，缺失时报告写“待 #84” |

每次运行创建 `acceptance/<yyyyMMdd-HHmmss>/`（已被 `.gitignore` 忽略），内含：

- `report.json`：证据报告（#87 契约），`coverage` 节来自 #84 的报告
- `run.log`：完整 gtest 输出
- `coverage-data/`：仅 Coverage 预设，`LLVM_PROFILE_FILE` 指向这里

退出码：报告 `verdict=passed` → `0`；`failed` / 报告缺失 / 二进制缺失 → `1`；参数错误 → `2`。

不经过脚本也可以直接跑（不设 `PRISM_E2E_REPORT` 时不写报告）：

```powershell
ctest --test-dir build/Windows/debug -L e2e --output-on-failure
.\build\Windows\debug\test\acceptance\test-acceptance.exe
```

## 报告契约

字段名即契约，不能随意改名：

```text
run      { commit, preset, mode, started_utc, duration_ms, media[] }
cases[]  { name, status(passed|failed|skipped), skip_reason, duration_ms,
           metrics { first_frame_ms, seek_ms, presented, dropped, waited,
                     cpu_user_s, peak_rss_mib, inter_client_skew_ms { p50, p95, max, samples } },
           evidence { log, repro } }
coverage { av_sync, service, threadpool }     # 每项：
         { status(measured|pending), line_percent, covered_lines,
           total_lines, expected_percent, reason }
verdict  passed | failed
```

约定：

- **失败用例必须带 `evidence.repro`**：一条可直接复制运行的命令，带上失败时的
  `PRISM_E2E_PRESET` / `PRISM_E2E_MODE` 与 `--gtest_filter=Suite.Case`，不需要重跑整套。
  例：`PRISM_E2E_PRESET='windows-clang-x64-Debug' PRISM_E2E_MODE='in-process' '/abs/test-acceptance' --gtest_filter=Suite.Case`。
- **跳过不是失败**：缺 GPU / 缺声卡 / 缺 Server 二进制 / 覆盖率工具未接入时，
  用例 `GTEST_SKIP() << "原因"`，`status=skipped` 并带上 `skip_reason`，
  `verdict` 仍可为 `passed`。只有 `failed` 会让 `verdict=failed`。
- **空报告不是绿**：一条用例都没执行（例如 `--gtest_filter` 写错）时 `verdict=failed`，
  避免“什么都没跑”被当成通过。
- 模式 A 只断言状态机与假引擎调用次数；位置推进、presented/dropped/waited
  要等引擎→AV Sync 回调（#55），所以 `metrics` 里这些字段目前是 `null` / `0`。
  `inter_client_skew_ms` 在未采集跨端样本时为 `null`。
- `run.media` 在模式 A 为空数组：用例打开的是合成地址（`acceptance://synthetic.mp4`），
  不落地真实媒体文件。
- **覆盖率只接入、不设阈值**：`coverage` 读 `PRISM_E2E_COVERAGE_REPORT`
  （默认 `build/<平台>/coverage/coverage-report.txt`，即 #84 的 `llvm-cov report` 产物），
  按批次累加行覆盖 —— `av_sync` = `business/av_sync/src/`、`service` = `service/src/`、
  `threadpool` = `threadpool/`（口径同 `docs/markdown/代码规范.md` §7.1）。
  `expected_percent=80` 只是标注，**不参与 verdict**；报告缺失或批次没有源文件时
  `status=pending`、`line_percent=null`、`reason` 写明原因（#84 未就绪时以「待 #84」开头），
  相关用例 skip 而不是 fail。
  `threadpool` 在 #84 当前口径下拿不到数字（口径排除 `vendor/`，且 `vendor/threadpool`
  不参与构建），报告里写「无法测量」而不是「待 #84」；若报告里出现 `threadpool/` 行会照常给出数字。

## 覆盖率（#93）

#84 产出覆盖率报告，本目录只消费它，不重算统计、不设阈值：

```powershell
pwsh -File scripts/build/coverage.ps1        # 配置 → 构建 → ctest → llvm-cov
pwsh -File scripts/e2e/run-acceptance.ps1    # 把报告接进 report.json 的 coverage 节
```

```bash
bash scripts/build/coverage.sh
bash scripts/e2e/run-acceptance.sh
```

报告路径默认 `build/<平台>/coverage/coverage-report.txt`，与验收预设的配置目录无关
（Debug 验收也能读到 Coverage 构建产出的报告）；换路径用 `--coverage-report <path>`。
`coverage.ps1` / `coverage.sh` 会跑全套 ctest，耗时较长；只验证接入时可直接复用已有报告文件。

## CTest 标签

| 标签 | 含义 | 默认是否跑 |
|------|------|-----------|
| `e2e` | 模式 A 验收（假引擎，无真实设备） | 是 |
| `e2e-multi` | 模式 B 多进程验收（#87 第 3 步） | 否（testPreset `filter.exclude`） |
| `perf` | 性能用例（#87 第 4 步） | 否 |
| `perf-long` | 长时性能用例 | 否 |

当前只有 `e2e` 标签有真实用例；`e2e-multi` / `perf` / `perf-long` 是**预留标签**：
`CMakePresets.json` 的 `filter.exclude` 已把它们排除，等 #87 第 3/4 步的用例落地时
直接挂标签即可，不会污染默认集合。

## 目录

```text
test/acceptance/
  main.cpp                     # 自定义入口：收集用例 → 写报告 → verdict 决定退出码
  test_lifecycle.cpp           # create / destroy / 版本号
  test_media_session.cpp       # open / close / MEDIA_LOADED / 引擎注入
  test_playback_control.cpp    # play / pause / stop / seek 状态与调用次数
  test_error_paths.cpp         # 空句柄、非法参数、无媒体、引擎初始化失败
  test_environment.cpp         # 缺件场景：带原因 skip
  test_report_contract.cpp     # 报告写入器自检（repro / skip_reason / coverage / verdict）
  test_coverage.cpp            # 覆盖率报告解析与分批聚合（#93）
  support/
    FakeEngines.h/.cpp         # 记录调用次数与参数的假音频/视频引擎与工厂
    PlayerHarness.h/.cpp       # 事件收集、句柄 RAII、假后端组装
    Report.h/.cpp              # 报告上下文、JSON 写入、GTest 监听器
    Coverage.h/.cpp            # #84 的 llvm-cov 文本报告解析与分批聚合
```
