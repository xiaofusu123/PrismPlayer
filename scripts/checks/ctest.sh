#!/bin/sh
# ctest 接入点（#102 / spec #96 决策 25）：回答「这次到底有没有跑测试」，不是「测试全过了吗」。
# 契约：test/ 没注册用例时明确 SKIP 并写明原因，不假装绿；根构建出现 add_test() 后自动变成真检查。
# 用法：ctest.sh <构建目录>
# 退出码：0 = 通过或明确 SKIP；1 = 有测试挂了，或无法判定（缺构建目录 / 没有测试清单）。

set -u

PRISM_CHECKS_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=scripts/checks/lib.sh
. "${PRISM_CHECKS_DIR}/lib.sh"

PRISM_BUILD_DIR=${1:-}
PRISM_STARTED=$(date +%s)
PRISM_ELAPSED=0

elapsed() {
    PRISM_ELAPSED=$(($(date +%s) - PRISM_STARTED))
}

# 写 GitHub job 摘要（#102 AC：运行时长与失败信息进 PR 摘要）。本机跑时该变量为空，静默跳过。
summary() {
    [ -n "${GITHUB_STEP_SUMMARY:-}" ] || return 0
    cat >> "${GITHUB_STEP_SUMMARY}"
}

if [ -z "${PRISM_BUILD_DIR}" ]; then
    prism_error "用法：ctest.sh <构建目录>（例：build/Linux/debug）"
    exit 1
fi

if [ ! -d "${PRISM_BUILD_DIR}" ]; then
    prism_error "找不到构建目录：${PRISM_BUILD_DIR}"
    prism_error "原因：ctest 要从构建树里读测试清单，没配置过就无从判定有没有测试。"
    prism_error "处理：跑一次 cmake --preset <平台>-clang-x64-Debug 生成它。"
    exit 1
fi

# 判定不能只看退出码：没有测试时 `ctest -N` 也以 0 结束，只看退出码等于把「什么都没跑」当绿灯。
PRISM_LIST=$(ctest --test-dir "${PRISM_BUILD_DIR}" -N 2>&1)
PRISM_LIST_STATUS=$?

if [ "${PRISM_LIST_STATUS}" -ne 0 ] || ! printf '%s\n' "${PRISM_LIST}" | grep -q '^Total Tests: '; then
    prism_error "无法从构建树里读出测试清单（ctest -N 返回 ${PRISM_LIST_STATUS}）。"
    prism_error "原始输出："
    printf '%s\n' "${PRISM_LIST}" | sed 's/^/[门禁]   /' >&2
    prism_error "处理：确认构建目录是刚配置过的（CTestTestfile.cmake 存在），再重试。"
    exit 1
fi

PRISM_TOTAL=$(printf '%s\n' "${PRISM_LIST}" | sed -n 's/^Total Tests: \([0-9][0-9]*\).*$/\1/p' | head -n 1)
PRISM_TOTAL=${PRISM_TOTAL:-0}

if [ "${PRISM_TOTAL}" -eq 0 ]; then
    elapsed
    prism_blank
    prism_info "SKIP：本次没有任何注册的测试用例（ctest 报 Total Tests: 0，耗时 ${PRISM_ELAPSED}s）。"
    prism_info "原因：test/ 还没接回根 CMake，注册测试属测试跑道票（#54）的范围，本 job 只预留接入点。"
    prism_info "这不是「测试都过了」，是「没有测试可跑」—— 所以用警告标注而不是静默绿灯。"
    prism_info "等 add_test() 出现，这个 job 会自动变成真检查，不需要改 CI。"
    # GitHub Actions 的 job 无法从步骤内部把自己标成 neutral；警告标注 + job 摘要是最接近「明确 SKIP」的
    # 诚实表达（#102 AC：不出现「假装绿」）。
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
        printf '::warning title=ctest SKIP::没有任何注册的测试用例（Total Tests: 0）；test/ 接回根构建后本 job 自动变成真检查（#54）。\n'
    fi
    summary <<EOF
### ctest：SKIP

本次没有任何注册的测试用例（\`ctest -N\` 报 \`Total Tests: 0\`，耗时 ${PRISM_ELAPSED}s）。

这不是「测试都过了」，是「没有测试可跑」。\`test/\` 接回根 CMake、出现 \`add_test()\` 之后
本 job 会自动变成真检查，不需要改 CI（#54 / spec #96 决策 25）。
EOF
    exit 0
fi

prism_info "ctest：${PRISM_TOTAL} 个注册的测试用例，开始运行。"
if ctest --test-dir "${PRISM_BUILD_DIR}" --output-on-failure; then
    elapsed
    prism_info "ctest 通过：${PRISM_TOTAL} 个测试全部通过（耗时 ${PRISM_ELAPSED}s）。"
    summary <<EOF
### ctest：通过

\`${PRISM_TOTAL}\` 个测试全部通过，耗时 ${PRISM_ELAPSED}s。
EOF
    exit 0
fi

# 失败时点名是哪条挂了，不把「有测试挂了」含糊成「这一步失败」。
elapsed
prism_blank
prism_error "ctest 未通过：${PRISM_TOTAL} 个测试里有失败的（耗时 ${PRISM_ELAPSED}s）。"
PRISM_FAILED_LOG="${PRISM_BUILD_DIR}/Testing/Temporary/LastTestsFailed.log"
if [ -f "${PRISM_FAILED_LOG}" ]; then
    prism_error "挂掉的测试："
    sed 's/^/[门禁]   /' "${PRISM_FAILED_LOG}" >&2
fi
prism_error "处理：本地复现 \`ctest --test-dir ${PRISM_BUILD_DIR} --output-on-failure\`，失败测试的完整输出见上面。"

{
    printf '### ctest：未通过\n\n'
    printf '\`%s\` 个测试里有失败的，耗时 %ss。\n\n' "${PRISM_TOTAL}" "${PRISM_ELAPSED}"
    if [ -f "${PRISM_FAILED_LOG}" ]; then
        printf '挂掉的测试：\n\n```\n'
        cat "${PRISM_FAILED_LOG}"
        printf '```\n'
    fi
} | summary

exit 1
