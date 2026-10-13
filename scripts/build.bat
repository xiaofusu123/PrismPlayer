@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

set "PROJECT_ROOT=%~dp0.."
echo project root: %PROJECT_ROOT%

cd "%PROJECT_ROOT%"

set "PREREQUISITE_DOC=docs\markdown\项目框架.md 7.1（环境与依赖基线）"

if not defined CLANG_ROOT (
    echo 错误：环境变量 CLANG_ROOT 未设置。前置条件与设置方式见 %PREREQUISITE_DOC%。
    exit /b 1
)
if not exist "%CLANG_ROOT%\clang.exe" (
    echo 错误：CLANG_ROOT 指向的目录里没有 clang.exe（当前值：%CLANG_ROOT%）。前置条件与设置方式见 %PREREQUISITE_DOC%。
    exit /b 1
)
if not defined VCPKG_ROOT (
    echo 错误：环境变量 VCPKG_ROOT 未设置。前置条件与设置方式见 %PREREQUISITE_DOC%。
    exit /b 1
)
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo 错误：VCPKG_ROOT 指向的目录不是 vcpkg checkout（当前值：%VCPKG_ROOT%）。前置条件与设置方式见 %PREREQUISITE_DOC%。
    exit /b 1
)
echo vcpkg root: %VCPKG_ROOT%

set "choice="
set "clean="
for %%a in (%*) do (
    if "%%a"=="--clean" set "clean=1"
    if "%%a"=="1" if not defined choice set "choice=1"
    if "%%a"=="2" if not defined choice set "choice=2"
)

if not defined choice (
    set /p "choice=请输入你要构建的预设(1.windows-clang-x64-Debug 2.windows-clang-x64-Release): "
)

if "!choice!"=="1" (
    set "preset=windows-clang-x64-Debug"
    set "builddir=build\Windows\debug"
) else if "!choice!"=="2" (
    set "preset=windows-clang-x64-Release"
    set "builddir=build\Windows\release"
) else (
    echo 无效的预设选项，默认使用 Release
    set "preset=windows-clang-x64-Release"
    set "builddir=build\Windows\release"
)

echo 使用的预设为 !preset!

if defined clean (
    if exist "!builddir!" (
        echo 清空构建目录（含依赖安装树 vcpkg_installed）：!builddir!
        rmdir /s /q "!builddir!"
    ) else (
        echo 构建目录不存在，无需清空：!builddir!
    )
    echo （.vcpkg-cache 作为本机加速缓存予以保留，见 %PREREQUISITE_DOC%）
)

cmake --preset !preset!

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
