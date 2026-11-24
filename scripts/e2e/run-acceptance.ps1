#!/usr/bin/env pwsh
<#
.SYNOPSIS
验收编排入口（Windows）：跑 test-acceptance，把证据报告写到带时间戳的运行目录。

.DESCRIPTION
与 scripts/e2e/run-acceptance.sh 在参数、默认值、退出码上完全一致：
  --preset <name>   构建/测试预设，默认 windows-clang-x64-Debug
  --mode a|b        验收模式，默认 a（in-process）；b 目前只会记录“未交付”的 skip
  --perf            请求性能用例（当前只会记录“未交付”的 skip）

退出码：报告 verdict=passed → 0；verdict=failed / 报告缺失 / 二进制缺失 → 1；参数错误 → 2。
缺 GPU / 声卡 / Server 二进制 / 覆盖率工具时，用例自行 skip（带原因），不算失败。
#>

$ErrorActionPreference = "Stop"

if (-not ($IsWindows -or $env:OS -eq "Windows_NT")) {
    Write-Host "错误：本脚本用于 Windows；Linux / WSL 请用 scripts/e2e/run-acceptance.sh" -ForegroundColor Red
    exit 2
}

$PROJECT_ROOT = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$Preset = "windows-clang-x64-Debug"
$Mode = "a"
$PerfRequested = $false

for ($i = 0; $i -lt $args.Count; $i++) {
    $arg = [string]$args[$i]
    if ($arg -ceq '--preset' -or $arg -clike '--preset=*') {
        if ($arg -ceq '--preset') {
            if ($i + 1 -ge $args.Count) { Write-Host "错误：--preset 缺少取值" -ForegroundColor Red; exit 2 }
            $Preset = [string]$args[++$i]
        } else {
            $Preset = $arg.Substring('--preset='.Length)
        }
    } elseif ($arg -ceq '--mode' -or $arg -clike '--mode=*') {
        if ($arg -ceq '--mode') {
            if ($i + 1 -ge $args.Count) { Write-Host "错误：--mode 缺少取值" -ForegroundColor Red; exit 2 }
            $Mode = [string]$args[++$i]
        } else {
            $Mode = $arg.Substring('--mode='.Length)
        }
    } elseif ($arg -ceq '--perf') {
        $PerfRequested = $true
    } elseif ($arg -ceq '-h' -or $arg -ceq '--help') {
        Write-Host "用法：pwsh -File scripts/e2e/run-acceptance.ps1 [--preset <name>] [--mode a|b] [--perf]"
        exit 0
    } else {
        Write-Host "错误：未知参数 $arg" -ForegroundColor Red
        exit 2
    }
}

if ($Mode -cne "a" -and $Mode -cne "b") {
    Write-Host "错误：--mode 只接受 a 或 b（当前：$Mode）" -ForegroundColor Red
    exit 2
}

if ($Preset -cnotmatch '^(windows|linux)-.+-x64-(Debug|Release|Coverage)$') {
    Write-Host "错误：无法识别的预设 $Preset（预期形如 windows-clang-x64-Debug）" -ForegroundColor Red
    exit 2
}
$platformToken = $Matches[1]
$configToken = $Matches[2]
if ($platformToken -cne "windows") {
    Write-Host "错误：本脚本用于 Windows 预设；$Preset 属于 Linux（请用 scripts/e2e/run-acceptance.sh）" -ForegroundColor Red
    exit 2
}

$platformDir = "Windows"
$configDir = $configToken.ToLowerInvariant()
$buildDir = Join-Path $PROJECT_ROOT "build/$platformDir/$configDir"
$binary = Join-Path $buildDir "test/acceptance/test-acceptance.exe"

if (-not (Test-Path $binary)) {
    Write-Host "错误：找不到 $binary" -ForegroundColor Red
    Write-Host "先构建：pwsh -File scripts/build/build.ps1 1   （1 = Debug）" -ForegroundColor Yellow
    exit 1
}

# ---- 运行目录（带时间戳；同秒内重复运行不会复用旧目录）----

$runsRoot = Join-Path $PROJECT_ROOT "acceptance"
New-Item -ItemType Directory -Force $runsRoot | Out-Null
do {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $runDir = Join-Path $runsRoot $stamp
} while (Test-Path $runDir)
New-Item -ItemType Directory -Path $runDir | Out-Null

$reportPath = Join-Path $runDir "report.json"
$logPath = Join-Path $runDir "run.log"

# ---- 环境：报告上下文由编排脚本注入（C++ 侧只读不猜）----

$commit = (& git -C $PROJECT_ROOT rev-parse HEAD 2>$null)
if (-not $commit) { $commit = "unknown" }

$env:PRISM_E2E_REPORT = $reportPath
$env:PRISM_E2E_PRESET = $Preset
$env:PRISM_E2E_MODE = if ($Mode -ceq "b") { "multi-process" } else { "in-process" }
$env:PRISM_E2E_COMMIT = ([string]$commit).Trim()
$env:PRISM_E2E_LOG = $logPath
$env:PRISM_E2E_PERF = if ($PerfRequested) { "1" } else { "0" }

if ($configDir -eq "coverage") {
    $profileDir = Join-Path $runDir "coverage-data"
    New-Item -ItemType Directory -Force $profileDir | Out-Null
    $env:LLVM_PROFILE_FILE = Join-Path $profileDir "test-%p-%m.profraw"
}

Write-Host "==> 预设 $Preset（$buildDir）"
Write-Host "==> 模式 $Mode，perf=$PerfRequested，运行目录 $runDir"
Write-Host "==> 执行 $binary"

Push-Location (Split-Path -Parent $binary)
try {
    & $binary --gtest_color=no 2>&1 | Tee-Object -FilePath $logPath
    $binaryRc = $LASTEXITCODE
} finally {
    Pop-Location
}

# ---- 判定：verdict 说话，退出码跟着 verdict ----

$verdict = ""
$counts = "（报告缺失，无法统计）"
if (Test-Path $reportPath) {
    try {
        $report = Get-Content -Raw $reportPath | ConvertFrom-Json
        $verdict = [string]$report.verdict
        $passed = @($report.cases | Where-Object { $_.status -eq "passed" }).Count
        $skipped = @($report.cases | Where-Object { $_.status -eq "skipped" }).Count
        $failed = @($report.cases | Where-Object { $_.status -eq "failed" }).Count
        $counts = "通过 $passed / 跳过 $skipped / 失败 $failed（共 $($report.cases.Count)）"
    } catch {
        $verdict = ""
    }
}

Write-Host ""
Write-Host "==> 报告 $reportPath"
Write-Host "==> 用例 $counts"
if ($binaryRc -ne 0) {
    Write-Host "==> 测试进程退出码 $binaryRc（详见 run.log）" -ForegroundColor Yellow
}

if ($verdict -eq "passed") {
    Write-Host "==> verdict passed" -ForegroundColor Green
    exit 0
}

if ($verdict -eq "failed") {
    Write-Host "==> verdict failed（失败用例的 evidence.repro 见报告）" -ForegroundColor Red
} else {
    Write-Host "==> 报告缺失或无法解析，判定失败" -ForegroundColor Red
}
exit 1
