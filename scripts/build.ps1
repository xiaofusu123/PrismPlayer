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

if ($args.Count -gt 0) {
    $choice = $($args[0])
} else {
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
