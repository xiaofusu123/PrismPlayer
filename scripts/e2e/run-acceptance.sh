#!/usr/bin/env bash
# 验收编排入口（Linux / WSL）：跑 test-acceptance，把证据报告写到带时间戳的运行目录。
#
# 与 scripts/e2e/run-acceptance.ps1 在参数、默认值、退出码上完全一致：
#   --preset <name>   构建/测试预设，默认 linux-clang-x64-Debug
#   --mode a|b        验收模式，默认 a（in-process）；b 目前只会记录“未交付”的 skip
#   --perf            请求性能用例（当前只会记录“未交付”的 skip）
#
# 退出码：报告 verdict=passed → 0；verdict=failed / 报告缺失 / 二进制缺失 → 1；参数错误 → 2。
# 缺 GPU / 声卡 / Server 二进制 / 覆盖率工具时，用例自行 skip（带原因），不算失败。

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
cd "$PROJECT_ROOT"

fail() {
    echo "错误：$*" >&2
    exit 2
}

case "$(uname -s)" in
Linux) ;;
*)
    echo "错误：本脚本用于 Linux / WSL；Windows 请用 scripts/e2e/run-acceptance.ps1" >&2
    exit 2
    ;;
esac

PRESET="linux-clang-x64-Debug"
MODE="a"
PERF_REQUESTED=0

while [ $# -gt 0 ]; do
    case "$1" in
    --preset)
        [ $# -ge 2 ] || fail "--preset 缺少取值"
        PRESET="$2"
        shift 2
        ;;
    --preset=*)
        PRESET="${1#*=}"
        shift
        ;;
    --mode)
        [ $# -ge 2 ] || fail "--mode 缺少取值"
        MODE="$2"
        shift 2
        ;;
    --mode=*)
        MODE="${1#*=}"
        shift
        ;;
    --perf)
        PERF_REQUESTED=1
        shift
        ;;
    -h | --help)
        echo "用法：bash scripts/e2e/run-acceptance.sh [--preset <name>] [--mode a|b] [--perf]"
        exit 0
        ;;
    *)
        fail "未知参数 $1"
        ;;
    esac
done

if [ "$MODE" != "a" ] && [ "$MODE" != "b" ]; then
    fail "--mode 只接受 a 或 b（当前：$MODE）"
fi

if ! [[ "$PRESET" =~ ^(windows|linux)-.+-x64-(Debug|Release|Coverage)$ ]]; then
    fail "无法识别的预设 $PRESET（预期形如 linux-clang-x64-Debug）"
fi
PLATFORM_TOKEN="${BASH_REMATCH[1]}"
CONFIG_TOKEN="${BASH_REMATCH[2]}"
if [ "$PLATFORM_TOKEN" != "linux" ]; then
    fail "本脚本用于 Linux 预设；$PRESET 属于 Windows（请用 scripts/e2e/run-acceptance.ps1）"
fi

PLATFORM_DIR="Linux"
CONFIG_DIR=$(printf '%s' "$CONFIG_TOKEN" | tr '[:upper:]' '[:lower:]')
BUILD_DIR="$PROJECT_ROOT/build/$PLATFORM_DIR/$CONFIG_DIR"
BINARY="$BUILD_DIR/test/acceptance/test-acceptance"

if [ ! -x "$BINARY" ]; then
    echo "错误：找不到 $BINARY" >&2
    echo "先构建：bash scripts/build/build.sh 1   （1 = Debug）" >&2
    exit 1
fi

# ---- 运行目录（带时间戳；同秒内重复运行不会复用旧目录）----

RUNS_ROOT="$PROJECT_ROOT/acceptance"
mkdir -p "$RUNS_ROOT"
while :; do
    STAMP=$(date +%Y%m%d-%H%M%S)
    RUN_DIR="$RUNS_ROOT/$STAMP"
    [ -e "$RUN_DIR" ] || break
    sleep 1
done
mkdir -p "$RUN_DIR"

REPORT_PATH="$RUN_DIR/report.json"
LOG_PATH="$RUN_DIR/run.log"

# ---- 环境：报告上下文由编排脚本注入（C++ 侧只读不猜）----

COMMIT=$(git -C "$PROJECT_ROOT" rev-parse HEAD 2>/dev/null || true)
[ -n "$COMMIT" ] || COMMIT="unknown"

export PRISM_E2E_REPORT="$REPORT_PATH"
export PRISM_E2E_PRESET="$PRESET"
if [ "$MODE" = "b" ]; then
    export PRISM_E2E_MODE="multi-process"
else
    export PRISM_E2E_MODE="in-process"
fi
export PRISM_E2E_COMMIT="$COMMIT"
export PRISM_E2E_LOG="$LOG_PATH"
export PRISM_E2E_PERF="$PERF_REQUESTED"

if [ "$CONFIG_DIR" = "coverage" ]; then
    PROFILE_DIR="$RUN_DIR/coverage-data"
    mkdir -p "$PROFILE_DIR"
    export LLVM_PROFILE_FILE="$PROFILE_DIR/test-%p-%m.profraw"
fi

echo "==> 预设 $PRESET（$BUILD_DIR）"
echo "==> 模式 $MODE，perf=$PERF_REQUESTED，运行目录 $RUN_DIR"
echo "==> 执行 $BINARY"

set +e
(cd "$(dirname "$BINARY")" && "$BINARY" --gtest_color=no) 2>&1 | tee "$LOG_PATH"
BINARY_RC=${PIPESTATUS[0]}
set -e

# ---- 判定：verdict 说话，退出码跟着 verdict ----

VERDICT=""
COUNTS="（报告缺失，无法统计）"
if [ -f "$REPORT_PATH" ]; then
    VERDICT=$(sed -n 's/.*"verdict": *"\([a-z]*\)".*/\1/p' "$REPORT_PATH" | tail -n 1)
    PASSED=$(grep -c '"status": "passed"' "$REPORT_PATH" || true)
    SKIPPED=$(grep -c '"status": "skipped"' "$REPORT_PATH" || true)
    FAILED=$(grep -c '"status": "failed"' "$REPORT_PATH" || true)
    TOTAL=$((PASSED + SKIPPED + FAILED))
    COUNTS="通过 $PASSED / 跳过 $SKIPPED / 失败 $FAILED（共 $TOTAL）"
fi

echo ""
echo "==> 报告 $REPORT_PATH"
echo "==> 用例 $COUNTS"
if [ "$BINARY_RC" -ne 0 ]; then
    echo "==> 测试进程退出码 $BINARY_RC（详见 run.log）"
fi

case "$VERDICT" in
passed)
    echo "==> verdict passed"
    exit 0
    ;;
failed)
    echo "==> verdict failed（失败用例的 evidence.repro 见报告）" >&2
    ;;
*)
    echo "==> 报告缺失或无法解析，判定失败" >&2
    ;;
esac
exit 1
