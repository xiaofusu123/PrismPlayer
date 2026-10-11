#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd .. && pwd)
echo "project root: ${PROJECT_ROOT}"
echo "build directory: ${PROJECT_ROOT}/build"

cd ${PROJECT_ROOT}

echo "请输入你要构建的预设(1.linux-clang-x64-Debug 2.linux-clang-x64-Release):"
read -r choice

case "${choice}" in
    1) preset="linux-clang-x64-Debug";;
    2) preset="linux-clang-x64-Release";;
    *) echo "无效的预设选项，默认使用 Release"; preset="linux-clang-x64-Release";;
esac

cmake --preset "${preset}"
echo "cmake构建成功"

cmake --build --preset "${preset}"
echo "编译成功"
