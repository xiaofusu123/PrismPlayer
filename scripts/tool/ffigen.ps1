#!/usr/bin/env pwsh
# 生成 ui/lib/ffi/ffi_bridge.dart
[CmdletBinding()]
param([string]$ClangRoot = $env:CLANG_ROOT)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$uiDir = Join-Path $repoRoot 'ui'
if (-not (Test-Path (Join-Path $uiDir 'pubspec.yaml'))) {
  throw "找不到 ui/pubspec.yaml（仓库根：$repoRoot）"
}
if ([string]::IsNullOrEmpty($ClangRoot)) {
  throw "CLANG_ROOT 未设置"
}
$ClangRoot = $ClangRoot.TrimEnd('\', '/')

$clangExe = Join-Path $ClangRoot 'clang.exe'
if (-not (Test-Path -LiteralPath $clangExe)) { $clangExe = Join-Path $ClangRoot 'clang' }
if (-not (Test-Path -LiteralPath $clangExe)) { throw "找不到 clang：$clangExe" }

$libclang = Join-Path $ClangRoot 'libclang.dll'
if (-not (Test-Path -LiteralPath $libclang)) {
  $libclang = Join-Path $ClangRoot 'libclang.so'
}
if (-not (Test-Path -LiteralPath $libclang)) { throw "找不到 libclang（$ClangRoot）" }

$resourceInclude = Join-Path (& $clangExe -print-resource-dir).Trim() 'include'
if (-not (Test-Path -LiteralPath $resourceInclude)) { throw "找不到 clang 资源目录：$resourceInclude" }

$yaml    = Join-Path $uiDir 'ffigen.yaml'
$tmpYaml = Join-Path $uiDir '.ffigen.generated.yaml'
if (-not (Test-Path -LiteralPath $yaml)) { throw "缺少配置文件：$yaml" }

$content = Get-Content -LiteralPath $yaml -Raw
$content += "`n# 以下由 ffigen.ps1 注入`nllvm-path:`n  - '$($libclang -replace '\\', '/')'`n"
Set-Content -LiteralPath $tmpYaml -Value $content -Encoding utf8

try {
  Push-Location $uiDir
  try {
    & dart run ffigen --config (Split-Path $tmpYaml -Leaf) `
        --compiler-opts "-isystem `"$resourceInclude`""
    if ($LASTEXITCODE -ne 0) { throw "ffigen 失败，退出码 $LASTEXITCODE" }
  } finally { Pop-Location }
} finally {
  Remove-Item -LiteralPath $tmpYaml -Force -ErrorAction SilentlyContinue
}

Write-Host "已生成 $uiDir\lib\ffi\ffi_bridge.dart" -ForegroundColor Green