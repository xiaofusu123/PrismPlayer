#!/usr/bin/env pwsh
# 覆盖率报告（LLVM source-based coverage，#84）。
#
# 一条命令：配置 → 构建 → 跑 ctest → 合并 .profraw → 生成文本报告。
# 报告写到 build/<Platform>/coverage/coverage-report.txt，同时打印到终端。
#
# 用法：
#   pwsh -File scripts/coverage.ps1
#
# 口径：统计的是项目自己的 C/C++ 源码；test/、vendor/、vcpkg_installed/、build/
# 全部排除（正则见下方 $IgnoreRegex）。默认构建并运行当前注册的全部测试。
# 覆盖率不设阈值、不阻断（第二阶段只出数字，见 docs/adr/0002-代码门禁.md）。

$ErrorActionPreference = "Stop"

$PROJECT_ROOT = Split-Path -Parent $PSScriptRoot
Set-Location $PROJECT_ROOT

$PREREQUISITE_DOC = "docs/markdown/项目框架.md 7.1（环境与依赖基线）"

if (-not ($IsWindows -or $env:OS -eq "Windows_NT")) {
    Write-Host "错误：coverage.ps1 只覆盖 Windows；WSL / Linux 请用 bash scripts/coverage.sh。" -ForegroundColor Red
    exit 1
}

$platform = "windows"
$platformDir = "Windows"

$preset = "$platform-clang-x64-Coverage"
$buildDir = Join-Path $PROJECT_ROOT "build/$platformDir/coverage"
$profileDir = Join-Path $buildDir "coverage-data"
$profData = Join-Path $buildDir "coverage.profdata"
$reportPath = Join-Path $buildDir "coverage-report.txt"

# 排除测试自身、第三方依赖、构建产物：只报项目实现。
$IgnoreRegex = '[/\\](test|vendor|vcpkg_installed|build)[/\\]'

function Fail([string]$Message) {
    Write-Host "错误：$Message" -ForegroundColor Red
    exit 1
}

function Resolve-PrismVcpkgRoot {
    $candidates = @($env:VCPKG_ROOT, $env:PRISM_VCPKG_ROOT)
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path (Join-Path $candidate "scripts/buildsystems/vcpkg.cmake"))) {
            return (Convert-Path $candidate)
        }
    }
    return $null
}

# llvm-cov / llvm-profdata 优先取 CLANG_ROOT 里的那一份（与编译器同源、版本一致），
# 再退到 PATH 与平台默认安装位置。
function Resolve-LlvmTool([string]$Name) {
    $file = "$Name.exe"
    $candidates = @()
    if ($env:CLANG_ROOT) {
        $candidates += (Join-Path $env:CLANG_ROOT $file)
    }
    $onPath = Get-Command $file -ErrorAction SilentlyContinue
    if ($onPath) {
        $candidates += $onPath.Source
    }
    $candidates += "D:\Program\App\msys64\mingw64\bin\$file"
    $candidates += "D:\Program\App\msys64\clang64\bin\$file"
    $candidates += "C:\Program Files\LLVM\bin\$file"
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Convert-Path $candidate)
        }
    }
    return $null
}

# ---- 前置条件（与 scripts/build.ps1 同一口径）----

$missingTools = @()
foreach ($tool in @("clang.exe", "clang++.exe", "ninja.exe")) {
    if (-not $env:CLANG_ROOT -or -not (Test-Path (Join-Path $env:CLANG_ROOT $tool))) {
        $missingTools += $tool
    }
}
if ($missingTools.Count -gt 0) {
    Fail "环境变量 CLANG_ROOT 未设置或无效（当前值：'$env:CLANG_ROOT'），缺少：$($missingTools -join '、')。前置条件见 $PREREQUISITE_DOC。"
}

$vcpkgRoot = Resolve-PrismVcpkgRoot
if (-not $vcpkgRoot) {
    Fail "环境变量 VCPKG_ROOT 未设置或无效（当前值：'$env:VCPKG_ROOT'），未找到 vcpkg checkout。前置条件见 $PREREQUISITE_DOC。"
}
$env:VCPKG_ROOT = $vcpkgRoot

$llvmCov = Resolve-LlvmTool "llvm-cov"
$llvmProfdata = Resolve-LlvmTool "llvm-profdata"
if (-not $llvmCov -or -not $llvmProfdata) {
    $missing = @()
    if (-not $llvmCov) { $missing += "llvm-cov" }
    if (-not $llvmProfdata) { $missing += "llvm-profdata" }
    $hint = "Windows 上它们随 MSYS2 包提供：pacman -S --needed mingw-w64-x86_64-llvm-tools mingw-w64-x86_64-compiler-rt（profile 运行时在 compiler-rt 里）。"
    Fail "找不到 LLVM 覆盖率工具：$($missing -join '、')。$hint"
}

Write-Host "llvm-cov：$llvmCov"
Write-Host "llvm-profdata：$llvmProfdata"

# ---- 构建 ----

# 构建期 gtest_discover_tests 会真的执行一次测试二进制（为了枚举用例），
# 这里把它的 profile 写到临时前缀，跑测试前再清掉，避免混进报告。
# 目录必须先建好：LLVM profile 运行时不会替我们创建目录，目录不存在时会静默丢弃 profile。
New-Item -ItemType Directory -Force $profileDir | Out-Null
$env:LLVM_PROFILE_FILE = Join-Path $profileDir "build-%p-%m.profraw"

Write-Host ""
Write-Host "==> 配置（$preset）"
& cmake --preset $preset
if ($LASTEXITCODE -ne 0) {
    Fail "cmake 配置失败（预设 $preset）。"
}

Write-Host ""
Write-Host "==> 构建"
& cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) {
    Fail "构建失败（预设 $preset）。"
}

# ---- 跑测试并收集 profile ----

if (Test-Path $profileDir) {
    Get-ChildItem $profileDir -Filter *.profraw -File -ErrorAction SilentlyContinue | Remove-Item -Force
} else {
    New-Item -ItemType Directory -Force $profileDir | Out-Null
}

Write-Host ""
Write-Host "==> 跑测试（profile 写到 $profileDir）"
& ctest --preset $preset
if ($LASTEXITCODE -ne 0) {
    Fail "测试没有全绿；覆盖率报告只在测试通过时有意义。"
}

$profraws = @(Get-ChildItem $profileDir -Filter *.profraw -File)
if ($profraws.Count -eq 0) {
    Fail "测试没有产生任何 .profraw（$profileDir）。检查 ctest 预设的环境变量 LLVM_PROFILE_FILE。"
}

Write-Host ""
Write-Host "==> 合并 profile（$($profraws.Count) 个 .profraw）"
& $llvmProfdata merge -sparse @($profraws.FullName) -o $profData
if ($LASTEXITCODE -ne 0) {
    Fail "llvm-profdata merge 失败。"
}

# ---- 报告 ----

# 覆盖率按二进制映射回源码：测试可执行文件 + 被插桩的项目共享库都要作为 object 传入。
$objects = @()
$testRoot = Join-Path $buildDir "test"
if (Test-Path $testRoot) {
    $objects += @(Get-ChildItem $testRoot -Recurse -File | Where-Object {
            $_.Name -like "test-*" -and
            (-not $IsWindows -or $_.Extension -eq ".exe") -and
            $_.FullName -notmatch "CMakeFiles"
        })
}
$clientRoot = Join-Path $buildDir "client"
if (Test-Path $clientRoot) {
    $objects += @(Get-ChildItem $clientRoot -Recurse -File | Where-Object {
            $_.Name -match "^libclient-.*\.(dll|so)$" -and
            $_.FullName -notmatch "CMakeFiles"
        })
}

if ($objects.Count -eq 0) {
    Fail "在 $buildDir 下找不到测试可执行文件（test-*）或插桩的 libclient-* 库。"
}

$objectPaths = @($objects | ForEach-Object { $_.FullName })

# llvm-cov 的位置参数只取第一个文件（其余会被静默忽略）；多二进制必须每个都用 --object 传入，
# 否则报告里会只剩测试可执行文件自己的代码，DLL / 静态库的源码整片消失。
$objectArgs = @()
foreach ($objectPath in $objectPaths) {
    $objectArgs += @("--object", $objectPath)
}

Write-Host "==> 生成报告（$($objectPaths.Count) 个 object）"
$report = & $llvmCov report @objectArgs -instr-profile $profData -ignore-filename-regex $IgnoreRegex 2>&1
$reportExit = $LASTEXITCODE
$reportText = ($report | Out-String)
$reportText | Set-Content -Path $reportPath -Encoding utf8
Write-Host $reportText

if ($reportExit -ne 0) {
    Fail "llvm-cov report 失败。"
}

Write-Host ""
Write-Host "报告：$reportPath"
Write-Host "口径：项目源码（排除 test/、vendor/、vcpkg_installed/、build/）；覆盖率不设阈值、不阻断。"
