@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

set "PROJECT_ROOT=%~dp0.."
echo project root: %PROJECT_ROOT%
echo build directory: %PROJECT_ROOT%build

cd %PROJECT_ROOT%

set /p "choice=请输入你要构建的预设(1.windows-clang-x64-Debug 2.windows-clang-x64-Release): "

if "%choice%"=="1" (
    set "preset=windows-clang-x64-Debug"
    cmake --preset !preset!
) else if "%preset%"=="2" (
    set "preset=windows-clang-x64-Release"
    cmake --preset !preset!
) else (
    echo 无效的预设选项，默认使用 Release
    set "preset=windows-clang-x64-Release"
    cmake --preset windows-clang-x64-Release
)

if errorlevel 1 (
    echo cmake构建失败
    exit /b 1
) else (
    echo cmake构建成功
)

cmake --build --preset !preset!

if errorlevel 1 (
    echo 编译失败
    exit /b 1
) else (
    echo 编译成功
)
