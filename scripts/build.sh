#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
echo "project root: ${PROJECT_ROOT}"

cd "${PROJECT_ROOT}"

PREREQUISITE_DOC="docs/markdown/项目框架.md 7.1（环境与依赖基线）"

if [ -z "${VCPKG_ROOT:-}" ] && [ -f "${PROJECT_ROOT}/scripts/wsl-env.sh" ]; then
    . "${PROJECT_ROOT}/scripts/wsl-env.sh"
fi
if [ -z "${VCPKG_ROOT:-}" ] || [ ! -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]; then
    echo "错误：环境变量 VCPKG_ROOT 未设置或无效（当前值：'${VCPKG_ROOT:-}'），未找到 vcpkg checkout。" >&2
    echo "前置条件与设置方式见 ${PREREQUISITE_DOC}；WSL 侧可用 scripts/wsl-env.sh。" >&2
    exit 1
fi
echo "vcpkg root: ${VCPKG_ROOT}"

CLEAN=0
CHOICE=""
for arg in "$@"; do
    case "${arg}" in
        --clean) CLEAN=1;;
        1|2) if [ -z "${CHOICE}" ]; then CHOICE="${arg}"; fi;;
    esac
done

if [ -z "${CHOICE}" ]; then
    echo "请输入你要构建的预设(1.linux-clang-x64-Debug 2.linux-clang-x64-Release):"
    read -r CHOICE
fi

case "${CHOICE}" in
    1) preset="linux-clang-x64-Debug";;
    2) preset="linux-clang-x64-Release";;
    *) echo "无效的预设选项，默认使用 Release"; preset="linux-clang-x64-Release";;
esac
echo "使用的预设为 ${preset}"

if [ "${CLEAN}" -eq 1 ]; then
    case "${preset}" in
        *Debug) build_dir="${PROJECT_ROOT}/build/Linux/debug";;
        *) build_dir="${PROJECT_ROOT}/build/Linux/release";;
    esac
    if [ -d "${build_dir}" ]; then
        echo "清空构建目录（含依赖安装树 vcpkg_installed）：${build_dir}"
        rm -rf "${build_dir}"
    else
        echo "构建目录不存在，无需清空：${build_dir}"
    fi
    echo "（.vcpkg-cache 作为本机加速缓存予以保留，见 ${PREREQUISITE_DOC}）"
fi

cmake --preset "${preset}"
echo "cmake构建成功"

cmake --build --preset "${preset}"
echo "编译成功"
