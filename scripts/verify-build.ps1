#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$PROJECT_ROOT = Split-Path -Parent $PSScriptRoot

if ($IsWindows -or $env:OS -eq "Windows_NT") {
    $platform = "windows"
} elseif ($IsLinux) {
    $platform = "linux"
} else {
    throw "The target platform isn't compatible"
}

$PREREQUISITE_DOC = "docs/markdown/项目框架.md 7.1（环境与依赖基线）"

$SPECS = @{
    "windows:debug" = @{
        BuildDir    = "build/Windows/debug"
        Triplet     = "x64-mingw-dynamic"
        MinPackages = 25
        Artifacts   = @(
            @{ Path = "client/engine/libclient-engine.dll"; MinSize = 50000 },
            @{ Path = "client/service/libclient-service.dll"; MinSize = 5000000 },
            @{ Path = "client/business/av_sync/libclient-av_sync.a"; MinSize = 7500000 },
            @{ Path = "server/Prism-server.exe"; MinSize = 8000 }
        )
    }
    "windows:release" = @{
        BuildDir    = "build/Windows/release"
        Triplet     = "x64-mingw-dynamic"
        MinPackages = 25
        Artifacts   = @(
            @{ Path = "client/engine/libclient-engine.dll"; MinSize = 9000 },
            @{ Path = "client/service/libclient-service.dll"; MinSize = 120000 },
            @{ Path = "client/business/av_sync/libclient-av_sync.a"; MinSize = 400000 },
            @{ Path = "server/Prism-server.exe"; MinSize = 8000 }
        )
    }
    "linux:debug" = @{
        BuildDir    = "build/Linux/debug"
        Triplet     = "x64-linux"
        MinPackages = 25
        Artifacts   = @(
            @{ Path = "client/engine/libclient-engine.so"; MinSize = 36000 },
            @{ Path = "client/service/libclient-service.so"; MinSize = 2000000 },
            @{ Path = "client/business/av_sync/libclient-av_sync.a"; MinSize = 6000000 },
            @{ Path = "server/Prism-server"; MinSize = 4000 }
        )
    }
    "linux:release" = @{
        BuildDir    = "build/Linux/release"
        Triplet     = "x64-linux"
        MinPackages = 25
        Artifacts   = @(
            @{ Path = "client/engine/libclient-engine.so"; MinSize = 6000 },
            @{ Path = "client/service/libclient-service.so"; MinSize = 120000 },
            @{ Path = "client/business/av_sync/libclient-av_sync.a"; MinSize = 490000 },
            @{ Path = "server/Prism-server"; MinSize = 4000 }
        )
    }
}

$script:Checks = 0
$script:Failures = 0

function Add-Check {
    param([bool]$Ok, [string]$Message)
    $script:Checks++
    if ($Ok) {
        Write-Host "  [PASS] $Message" -ForegroundColor Green
    } else {
        $script:Failures++
        Write-Host "  [FAIL] $Message" -ForegroundColor Red
    }
}

function Find-Objdump {
    $candidates = @()
    if ($env:CLANG_ROOT) { $candidates += (Join-Path $env:CLANG_ROOT "objdump.exe") }
    $candidates += "objdump"
    foreach ($candidate in $candidates) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    return $null
}

function Test-Config {
    param([string]$Key)
    $spec = $SPECS[$Key]
    $dir = Join-Path $PROJECT_ROOT $spec.BuildDir
    Write-Host ""
    Write-Host "=== 预设 $Key（$($spec.BuildDir)）===" -ForegroundColor Cyan

    if (-not (Test-Path $dir)) {
        Add-Check $false "构建目录存在：$($spec.BuildDir)"
        return
    }
    Add-Check $true "构建目录存在：$($spec.BuildDir)"

    $status = Join-Path $dir "vcpkg_installed/vcpkg/status"
    $pkgCount = 0
    if (Test-Path $status) {
        $pkgCount = (Select-String -Path $status -Pattern "^Package: " | Measure-Object).Count
    }
    Add-Check (Test-Path $status) "依赖安装树状态文件存在：vcpkg_installed/vcpkg/status"
    Add-Check ($pkgCount -ge $spec.MinPackages) "安装树里有 $pkgCount 个已安装包（阈值 $($spec.MinPackages)）"

    $libDir = Join-Path $dir "vcpkg_installed/$($spec.Triplet)/lib"
    $libCount = 0
    if (Test-Path $libDir) {
        $libCount = (Get-ChildItem -File -Recurse $libDir |
            Where-Object { $_.Name -like "*.dll.a" -or $_.Name -like "*.so*" -or $_.Extension -in ".a", ".lib" }).Count
    }
    Add-Check ($libCount -ge 10) "安装树里有 $libCount 个库/导入库文件（阈值 10，位于 $($spec.Triplet)/lib）"

    foreach ($artifact in $spec.Artifacts) {
        $path = Join-Path $dir $artifact.Path
        if (-not (Test-Path $path)) {
            Add-Check $false "产物存在：$($artifact.Path)（缺失）"
            continue
        }
        $size = (Get-Item $path).Length
        Add-Check ($size -ge $artifact.MinSize) ("产物 {0} = {1:N0} B（阈值 {2:N0} B）" -f $artifact.Path, $size, $artifact.MinSize)
    }

    $serviceLib = if ($platform -eq "windows") { "client/service/libclient-service.dll" } else { "client/service/libclient-service.so" }
    $servicePath = Join-Path $dir $serviceLib
    $objdump = Find-Objdump
    if (-not $objdump) {
        Write-Host "  [SKIP] 未找到 objdump，跳过导出/导入检查" -ForegroundColor Yellow
        return
    }
    if (-not (Test-Path $servicePath)) {
        Add-Check $false "服务动态库存在，可检查导出：$serviceLib"
        return
    }

    $dump = & $objdump -p $servicePath 2>$null
    $symbols = if ($platform -eq "windows") { $dump } else { & $objdump -T $servicePath 2>$null }
    $exports = ($symbols | Select-String -Pattern "prism_player_" | Measure-Object).Count
    Add-Check ($exports -ge 32) "服务动态库导出 $exports 个 prism_player_* 符号（阈值 32）"

    $neededPattern = if ($platform -eq "windows") { "DLL Name: .*libc\+\+" } else { "NEEDED.*libc\+\+" }
    $abiSplit = ($dump | Select-String -Pattern $neededPattern | Measure-Object).Count
    Add-Check ($abiSplit -eq 0) "服务动态库未依赖 libc++（跨标准库 ABI 分裂会让链接失败，见 $PREREQUISITE_DOC）"
}

$selected = @()
foreach ($arg in $args) {
    if ($arg -eq 1) { $selected += "debug" }
    elseif ($arg -eq 2) { $selected += "release" }
}
if ($selected.Count -eq 0) { $selected = @("debug", "release") }

Write-Host "构建产物验收（#66 AC4）：平台 $platform，检查 $($selected -join "、")"
Write-Host "体积阈值刻意高于空归档/占位文件，并低于实测产物体积；实测值见 $PREREQUISITE_DOC"

foreach ($config in $selected) {
    Test-Config "$platform`:$config"
}

Write-Host ""
if ($script:Failures -eq 0) {
    Write-Host "验收通过：$($script:Checks) 项检查全部通过（平台 $platform）" -ForegroundColor Green
    exit 0
} else {
    Write-Host "验收失败：$($script:Failures) / $($script:Checks) 项不通过（平台 $platform）" -ForegroundColor Red
    exit 1
}
