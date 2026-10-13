#!/usr/bin/env pwsh
# 一条命令装上门禁（#67）：把 core.hooksPath 指向版本控制里的 .githooks/。
# 用 hooksPath 而不是往 .git/hooks/ 拷贝：拷贝出来的钩子不进版本控制，别人 clone 完是裸奔的。
# 用法：pwsh -File scripts/install-hooks.ps1

$ErrorActionPreference = "Stop"

$PROJECT_ROOT = Split-Path -Parent $PSScriptRoot
Set-Location $PROJECT_ROOT

# 从 Git Bash 里起的 pwsh 可能带着 PATHEXT=.CPL，这时按名字找 git 会扑空，退到 git.exe。
$gitExe = "git"
if (-not (Get-Command $gitExe -ErrorAction SilentlyContinue)) {
    $gitExe = "git.exe"
    if (-not (Get-Command $gitExe -ErrorAction SilentlyContinue)) {
        Write-Host "错误：PATH 上找不到 git，先装 Git for Windows 并把它加入 PATH。" -ForegroundColor Red
        exit 1
    }
}

$hooks = @("pre-commit", "pre-push", "check-clang-tidy.sh", "common.sh")

$missing = @()
foreach ($hook in @("pre-commit", "pre-push")) {
    if (-not (Test-Path (Join-Path ".githooks" $hook))) { $missing += $hook }
}
if ($missing.Count -gt 0) {
    Write-Host "错误：找不到 .githooks/$($missing -join '、')，当前目录 $PROJECT_ROOT 可能不是仓库根。" -ForegroundColor Red
    exit 1
}

& $gitExe config core.hooksPath .githooks

# 判据是读回来的值，不是 $LASTEXITCODE：从 Git Bash 里起的 pwsh 拿不到它（会是空值），
# 拿它当判据会在写成功时误报失败。
$hooksPath = & $gitExe config --local --get core.hooksPath
if ($hooksPath -ne ".githooks") {
    Write-Host "错误：core.hooksPath 没设成 .githooks（读回来是 '$hooksPath'）。" -ForegroundColor Red
    exit 1
}

# CRLF 的 `#!/bin/sh` 在 Linux/WSL 上是 bad interpreter，钩子连版本检查都跑不到。
$crlf = @()
foreach ($hook in $hooks) {
    $path = Join-Path ".githooks" $hook
    if ((Test-Path $path) -and ((Get-Content -Raw $path) -match "`r`n")) { $crlf += $hook }
}
if ($crlf.Count -gt 0) {
    Write-Host "警告：$($crlf -join '、') 是 CRLF 行尾，Linux/WSL 下会 bad interpreter。" -ForegroundColor Yellow
    Write-Host "      .gitattributes 已声明 .githooks/* text eol=lf，重新 checkout 即可修正。" -ForegroundColor Yellow
}

Write-Host "门禁已装上：core.hooksPath = $hooksPath（写在本仓库 .git/config，不进版本控制）"
Write-Host ""
Write-Host "pre-commit  staged 的 C/C++ 文件必须 clang-format 过；文件名不得是 CmakeLists.txt（vendor/** 例外）。"
Write-Host "pre-push    先跑 windows-clang-x64-Debug 构建，再对本次推送改动的 C/C++ 文件跑 clang-tidy。"
Write-Host "版本要求     clang-format / clang-tidy 必须 23.1.x，不符时钩子在动手前就报错退出（ADR 决策 6）。"
Write-Host "唯一绕过场景 一次性格式批量整理，且必须在 PR 描述里说明：git commit/push --no-verify"

exit 0
