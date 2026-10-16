#!/usr/bin/env bash
# Linux CI 上的构建编排（spec #96 决策 8）：workflow 只做编排，预设名、失败提示、本地复现命令
# 都只写一遍 —— build / tidy / ctest 三个 job 调的是同一份。
#
# 用法：bash scripts/ci/preset-linux-debug.sh <configure|build|verify>
#   configure  按 linux-clang-x64-Debug 预设配置（产出 compile_commands.json，并把 vcpkg 依赖装进
#              build/Linux/debug/vcpkg_installed；命中二进制缓存时只要几十秒，未命中要编译依赖）
#   build      构建该预设
#   verify     跑既有的产物契约断言（scripts/verify-build.sh 1）

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "${PROJECT_ROOT}"

PRESET=linux-clang-x64-Debug
STEP=${1:-}

say() { printf '%s\n' "$*"; }

usage() {
    printf '用法：bash scripts/ci/preset-linux-debug.sh <configure|build|verify>\n' >&2
    exit 2
}

on_error() {
    printf '\n' >&2
    printf '错误：%s 这一步没通过（预设 %s）。\n' "${STEP}" "${PRESET}" >&2
    printf '这不是编排故障。按「先看日志里点名的东西，再怀疑代码」的顺序查：\n' >&2
    printf '  ① 日志里有 "requires the following programs from the system package manager" ——\n' >&2
    printf '     干净 runner 缺系统工具（autoreconf 一族，如 autoconf-archive / bison / flex；\n' >&2
    printf '     完整清单在 scripts/ci/setup-linux.sh 的 system 组件里，本机装着不代表 runner 上有）。\n' >&2
    printf '  ② 日志里有 "Cache not found for input keys" —— vcpkg 二进制缓存未命中，只是要重新编译\n' >&2
    printf '     依赖，不是失败；「键为什么变了」的对照表见 docs/adr/0006-门禁第二阶段CI.md 决策 5。\n' >&2
    printf '  ③ 日志里是 clang++ / 链接器的错误 —— 那才是代码本身编译不过。\n' >&2
    printf '本地复现（WSL / Linux，仓库根）：\n' >&2
    printf '  bash scripts/ci/setup-linux.sh llvm cmake system vcpkg\n' >&2
    printf '  bash scripts/build.sh 1 && bash scripts/verify-build.sh 1\n' >&2
    printf '前置条件与依赖基线：docs/markdown/项目框架.md 7.1\n' >&2
}
trap 'on_error' ERR

case "${STEP}" in
    configure)
        say "配置：cmake --preset ${PRESET}"
        say "（首次要在干净 runner 上编译 vcpkg 依赖；命中二进制缓存后只要几十秒）"
        cmake --preset "${PRESET}"
        ;;
    build)
        say "构建：cmake --build --preset ${PRESET}"
        cmake --build --preset "${PRESET}"
        ;;
    verify)
        say "产物契约断言：bash scripts/verify-build.sh 1"
        bash scripts/verify-build.sh 1
        ;;
    *)
        usage
        ;;
esac
