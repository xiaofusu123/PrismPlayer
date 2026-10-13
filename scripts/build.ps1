#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$PROJECT_ROOT = Split-Path -Parent $PSScriptRoot

Write-Host "project root: $PROJECT_ROOT"

Set-Location $PROJECT_ROOT

if ($IsWindows -or $env:OS -eq "Windows_NT") {
    $platform = "windows"
} elseif ($IsLinux) {
    $platform = "linux"
} else {
    throw "The target platform isn't compatible"
}

Write-Host "compilation platform: $platform"

$PREREQUISITE_DOC = "docs/markdown/项目框架.md 7.1（环境与依赖基线）"

function Resolve-PrismVcpkgRoot {
    $candidates = @($env:VCPKG_ROOT, $env:PRISM_VCPKG_ROOT)
    if ($platform -eq "linux" -and $HOME) {
        $candidates += (Join-Path $HOME "vcpkg")
    }
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path (Join-Path $candidate "scripts/buildsystems/vcpkg.cmake"))) {
            return (Convert-Path $candidate)
        }
    }
    return $null
}

if ($platform -eq "windows") {
    $missingTools = @()
    foreach ($tool in @("clang.exe", "clang++.exe", "ninja.exe")) {
        if (-not $env:CLANG_ROOT -or -not (Test-Path (Join-Path $env:CLANG_ROOT $tool))) {
            $missingTools += $tool
        }
    }
    if ($missingTools.Count -gt 0) {
        Write-Host "错误：环境变量 CLANG_ROOT 未设置或无效（当前值：'$env:CLANG_ROOT'），缺少：$($missingTools -join '、')。" -ForegroundColor Red
        Write-Host "前置条件与设置方式见 $PREREQUISITE_DOC。" -ForegroundColor Red
        exit 1
    }
}

$vcpkgRoot = Resolve-PrismVcpkgRoot
if (-not $vcpkgRoot) {
    Write-Host "错误：环境变量 VCPKG_ROOT 未设置或无效（当前值：'$env:VCPKG_ROOT'），未找到 vcpkg checkout。" -ForegroundColor Red
    Write-Host "前置条件与设置方式见 $PREREQUISITE_DOC。" -ForegroundColor Red
    exit 1
}
if (-not $env:VCPKG_ROOT) {
    Write-Host "提示：未设置 VCPKG_ROOT，使用探测到的 vcpkg：$vcpkgRoot（见 $PREREQUISITE_DOC）"
}
$env:VCPKG_ROOT = $vcpkgRoot
Write-Host "vcpkg root: $vcpkgRoot"

$clean = $false
$choice = $null
foreach ($arg in $args) {
    if ($arg -eq "--clean") {
        $clean = $true
    } elseif ($arg -match "^[12]$" -and -not $choice) {
        $choice = $arg
    }
}

if (-not $choice) {
    $choice = Read-Host "请输入你要构建的预设(1.clang-x64-Debug 2.clang-x64-Release)"
}

if ($choice -eq 1) {
    $preset = "$platform-clang-x64-Debug"
} elseif ($choice -eq 2) {
    $preset = "$platform-clang-x64-Release"
} else {
    Write-Host "无效的预设选项，默认使用 Release"
    $preset = "$platform-clang-x64-Release"
}

Write-Host "使用的预设为 $preset"

if ($clean) {
    $platformDir = if ($platform -eq "windows") { "Windows" } else { "Linux" }
    $configDir = if ($preset -like "*Debug") { "debug" } else { "release" }
    $buildDir = Join-Path $PROJECT_ROOT "build/$platformDir/$configDir"
    if (Test-Path $buildDir) {
        Write-Host "清空构建目录（含依赖安装树 vcpkg_installed）：$buildDir"
        Remove-Item -Recurse -Force $buildDir
    } else {
        Write-Host "构建目录不存在，无需清空：$buildDir"
    }
    Write-Host "（.vcpkg-cache 作为本机加速缓存予以保留，见 $PREREQUISITE_DOC）"
}

cmake --preset $preset
if ($LASTEXITCODE -ne 0) {
    throw "cmake构建失败"
} else {
    Write-Host "cmake构建成功"
}

cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) {
    throw "编译失败"
} else {
    Write-Host "编译成功"
}
