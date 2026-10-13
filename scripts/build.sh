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

if [ $# -gt 0 ]; then
    choice=$1
else
    echo "请输入你要构建的预设(1.linux-clang-x64-Debug 2.linux-clang-x64-Release):"
    read -r choice
fi

case "${choice}" in
    1) preset="linux-clang-x64-Debug";;
    2) preset="linux-clang-x64-Release";;
    *) echo "无效的预设选项，默认使用 Release"; preset="linux-clang-x64-Release";;
esac

cmake --preset "${preset}"
echo "cmake构建成功"

cmake --build --preset "${preset}"
echo "编译成功"
