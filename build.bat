@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

set "PROJECT_ROOT=%~dp0"
echo project root: %PROJECT_ROOT%
echo build directory: %PROJECT_ROOT%build

set /p "preset=请输入你要构建的预设(1.windows-x64-Debug 2.windows-x64-Release): "

if "%preset%"=="1" (
    set "build_preset=windows-x64-Debug"
    cmake --preset !build_preset!
) else if "%preset%"=="2" (
    set "build_preset=windows-x64-Release"
    cmake --preset !build_preset!
) else (
    echo 无效的预设选项，默认使用 Release。
    cmake --preset windows-x64-Release
)

if errorlevel 1 (
    echo cmake配置失败
    exit /b 1
) else (
    echo cmake配置成功
)

cmake --build --preset !build_preset!

if errorlevel 1 (
    echo 构建失败
    exit /b 1
) else (
    echo 构建成功
)
