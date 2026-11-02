#!/usr/bin/env bash
# 覆盖率报告（LLVM source-based coverage，#84）。
#
# 一条命令：配置 → 构建 → 跑 ctest → 合并 .profraw → 生成文本报告。
# 报告写到 build/Linux/coverage/coverage-report.txt，同时打印到终端。
#
# 用法（WSL / Linux 仓库根）：
#   bash scripts/coverage.sh
#
# 口径：统计的是项目自己的 C/C++ 源码；test/、vendor/、vcpkg_installed/、build/
# 全部排除（正则见下方 IGNORE_REGEX）。默认构建并运行当前注册的全部测试。
# 覆盖率不设阈值、不阻断（第二阶段只出数字，见 docs/adr/0002-代码门禁.md）。

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "${PROJECT_ROOT}"

PREREQUISITE_DOC="docs/markdown/项目框架.md 7.1（环境与依赖基线）"
PLATFORM_DIR="Linux"
PRESET="linux-clang-x64-Coverage"
# 不重复钉版本：从 clang 自己的版本号推主版本，退回 23（与门禁的 23.1 前缀一致）。
LLVM_MAJOR=$(clang --version 2>/dev/null | sed -n '1s/.*version \([0-9][0-9]*\).*/\1/p' || true)
LLVM_MAJOR=${LLVM_MAJOR:-23}
BUILD_DIR="${PROJECT_ROOT}/build/${PLATFORM_DIR}/coverage"
PROFILE_DIR="${BUILD_DIR}/coverage-data"
PROF_DATA="${BUILD_DIR}/coverage.profdata"
REPORT="${BUILD_DIR}/coverage-report.txt"
# 排除测试自身、第三方依赖、构建产物：只报项目实现。
IGNORE_REGEX='[/\\](test|vendor|vcpkg_installed|build)[/\\]'

fail() {
    echo "错误：$*" >&2
    exit 1
}

if [ "$(uname -s)" != "Linux" ]; then
    fail "不支持的平台：$(uname -s)（本脚本用于 WSL / Linux；Windows 用 scripts/coverage.ps1）"
fi

# ---- 前置条件（与 scripts/build.sh 同一口径）----

if [ -z "${VCPKG_ROOT:-}" ] && [ -f "${PROJECT_ROOT}/scripts/wsl-env.sh" ]; then
    . "${PROJECT_ROOT}/scripts/wsl-env.sh"
fi
if [ -z "${VCPKG_ROOT:-}" ] || [ ! -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]; then
    fail "环境变量 VCPKG_ROOT 未设置或无效（当前值：'${VCPKG_ROOT:-}'），未找到 vcpkg checkout。前置条件与设置方式见 ${PREREQUISITE_DOC}；WSL 侧可用 scripts/wsl-env.sh。"
fi
echo "vcpkg root: ${VCPKG_ROOT}"

# llvm-cov / llvm-profdata 优先取 CLANG_ROOT 里的那一份（与编译器同源、版本一致），
# 再退到 PATH 与平台默认安装位置。
find_llvm_tool() {
    local name="$1" candidate
    if [ -n "${CLANG_ROOT:-}" ] && [ -x "${CLANG_ROOT}/${name}" ]; then
        printf '%s\n' "${CLANG_ROOT}/${name}"
        return 0
    fi
    candidate=$(command -v "${name}" 2>/dev/null || true)
    if [ -n "${candidate}" ]; then
        printf '%s\n' "${candidate}"
        return 0
    fi
    for candidate in "/usr/lib/llvm-${LLVM_MAJOR}/bin/${name}" "/usr/bin/${name}"; do
        if [ -x "${candidate}" ]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done
    return 1
}

LLVM_COV=$(find_llvm_tool llvm-cov || true)
LLVM_PROFDATA=$(find_llvm_tool llvm-profdata || true)
if [ -z "${LLVM_COV}" ] || [ -z "${LLVM_PROFDATA}" ]; then
    MISSING=""
    [ -z "${LLVM_COV}" ] && MISSING="${MISSING}llvm-cov "
    [ -z "${LLVM_PROFDATA}" ] && MISSING="${MISSING}llvm-profdata "
    fail "找不到 LLVM 覆盖率工具：${MISSING}。Ubuntu 上由 llvm-${LLVM_MAJOR} 包提供：sudo apt-get install -y llvm-${LLVM_MAJOR}（或确认 CLANG_ROOT 指向 /usr/lib/llvm-${LLVM_MAJOR}/bin）。"
fi
echo "llvm-cov：${LLVM_COV}"
echo "llvm-profdata：${LLVM_PROFDATA}"

# ---- 构建 ----

# 构建期 gtest_discover_tests 会真的执行一次测试二进制（为了枚举用例），
# 这里把它的 profile 写到临时前缀，跑测试前再清掉，避免混进报告。
# 目录必须先建好：LLVM profile 运行时不会替我们创建目录，目录不存在时会静默丢弃 profile。
mkdir -p "${PROFILE_DIR}"
export LLVM_PROFILE_FILE="${PROFILE_DIR}/build-%p-%m.profraw"

echo
echo "==> 配置（${PRESET}）"
cmake --preset "${PRESET}"

echo
echo "==> 构建"
cmake --build --preset "${PRESET}"

# ---- 跑测试并收集 profile ----

mkdir -p "${PROFILE_DIR}"
rm -f "${PROFILE_DIR}"/*.profraw 2>/dev/null || true

echo
echo "==> 跑测试（profile 写到 ${PROFILE_DIR}）"
ctest --preset "${PRESET}"

mapfile -t PROFRAWS < <(find "${PROFILE_DIR}" -maxdepth 1 -type f -name '*.profraw' | sort)
if [ "${#PROFRAWS[@]}" -eq 0 ]; then
    fail "测试没有产生任何 .profraw（${PROFILE_DIR}）。检查 ctest 预设的环境变量 LLVM_PROFILE_FILE。"
fi

echo
echo "==> 合并 profile（${#PROFRAWS[@]} 个 .profraw）"
"${LLVM_PROFDATA}" merge -sparse "${PROFRAWS[@]}" -o "${PROF_DATA}"

# ---- 报告 ----

# 覆盖率按二进制映射回源码：测试可执行文件 + 被插桩的项目共享库都要作为 object 传入。
mapfile -t OBJECTS < <(
    {
        find "${BUILD_DIR}/test" -type f -name 'test-*' -executable 2>/dev/null
        find "${BUILD_DIR}/client" -type f \( -name 'libclient-*.so' -o -name 'libclient-*.dll' \) 2>/dev/null
    } | sort
)
if [ "${#OBJECTS[@]}" -eq 0 ]; then
    fail "在 ${BUILD_DIR} 下找不到测试可执行文件（test-*）或插桩的 libclient-* 库。"
fi

echo
echo "==> 生成报告（${#OBJECTS[@]} 个 object）"
# llvm-cov 的位置参数只取第一个文件（其余会被静默忽略）；多二进制必须每个都用 --object 传入，
# 否则报告里会只剩测试可执行文件自己的代码，共享库 / 静态库的源码整片消失。
COV_ARGS=()
for object in "${OBJECTS[@]}"; do
    COV_ARGS+=(--object "${object}")
done
"${LLVM_COV}" report "${COV_ARGS[@]}" -instr-profile "${PROF_DATA}" \
    -ignore-filename-regex "${IGNORE_REGEX}" | tee "${REPORT}"

echo
echo "报告：${REPORT}"
echo "口径：项目源码（排除 test/、vendor/、vcpkg_installed/、build/）；覆盖率不设阈值、不阻断。"
