#!/usr/bin/env bash
# Linux CI 的构建编排（spec #96 决策 8）：把 workflow 里的构建步骤收敛成一份脚本。
# 契约：预设名、失败提示、本地复现命令都只写一遍，build / tidy / ctest 三个 job 调的是同一份。
# 用法：bash scripts/ci/preset-linux-debug.sh <configure|build|verify>。
# 退出码：0 通过；任一步失败或产物契约断言不通过则非 0，提示见 on_error。

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

# 预设（CMakePresets.json 的 linux-base）写死裸 `clang` / `clang++`，所以先问 PATH 上第一个 clang 是哪一版。
# 这是「llvm-23 真的排到最前了吗」的现场证据：#99 的第一次真实运行就是拿着 18 一路配置下去的。
check_compiler() {
    local prefix version
    prefix=$(sed -n 's/^[[:space:]]*PRISM_CLANG_VERSION_PREFIX="\([^"]*\)".*$/\1/p' \
        "${PROJECT_ROOT}/scripts/checks/lib.sh" | head -n 1)
    [ -n "${prefix}" ] || { printf '错误：从 scripts/checks/lib.sh 里读不到 PRISM_CLANG_VERSION_PREFIX。\n' >&2; exit 1; }

    version=$(clang --version 2>/dev/null | head -n 1) || true
    [ -n "${version}" ] || { printf '错误：PATH 上没有 clang。\n' >&2; exit 1; }

    case "${version}" in
        *"clang version ${prefix}"*)
            say "编译器：$(command -v clang) → ${version}"
            ;;
        *)
            printf '错误：PATH 上的 clang 是「%s」，不是 %s.x。\n' "${version}" "${prefix}" >&2
            printf '      linux-clang-x64-Debug 预设用的是裸 clang / clang++，这会直接影响构建结果。\n' >&2
            printf '      先跑：bash scripts/ci/setup-linux.sh llvm（把 llvm-%s 放到 PATH 最前面）。\n' \
                "${prefix%%.*}" >&2
            exit 1
            ;;
    esac
}

check_compiler

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
