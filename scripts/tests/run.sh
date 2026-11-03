#!/bin/sh
# 一次跑完两套门禁自测：钩子/安装脚本 + 检查入口。
# 用法：sh scripts/tests/run.sh（CI 仍分成两个 job 并行跑）。

set -u

PRISM_TEST_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "${PRISM_TEST_ROOT}" || exit 1

status=0
printf '==== 钩子与安装脚本 ====\n'
sh "${PRISM_TEST_ROOT}/scripts/tests/hooks.sh" || status=1
printf '\n==== 检查入口 ====\n'
sh "${PRISM_TEST_ROOT}/scripts/tests/checks.sh" || status=1
exit "${status}"
