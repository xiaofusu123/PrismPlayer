#!/bin/sh
# 一条命令装上门禁（#67）：把 core.hooksPath 指向版本控制里的 .githooks/。
# 用 hooksPath 而不是往 .git/hooks/ 拷贝：拷贝出来的钩子不进版本控制，别人 clone 完是裸奔的。
# 用法：sh scripts/install-hooks.sh　（Windows 请用 scripts/install-hooks.ps1）

set -u

PRISM_INSTALL_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "${PRISM_INSTALL_ROOT}" || exit 1

PRISM_FAIL=0
prism_say() { printf '%s\n' "$*"; }
prism_bad() { printf '%s\n' "$*" >&2; PRISM_FAIL=1; }

for PRISM_HOOK in pre-commit pre-push; do
    if [ ! -f ".githooks/${PRISM_HOOK}" ]; then
        prism_bad "错误：找不到 .githooks/${PRISM_HOOK}（当前目录 ${PRISM_INSTALL_ROOT} 可能不是仓库根）。"
    fi
done
[ "${PRISM_FAIL}" -eq 0 ] || exit 1

# 执行位：WSL/Linux 上缺了 git 会静默忽略钩子；Windows 上 chmod 是空操作。
chmod +x .githooks/pre-commit .githooks/pre-push .githooks/check-clang-tidy.sh .githooks/common.sh 2>/dev/null || :

if ! git config core.hooksPath .githooks; then
    prism_bad "错误：git config core.hooksPath .githooks 执行失败。"
    exit 1
fi

# 校验真的生效。
PRISM_SET=$(git config --local --get core.hooksPath 2>/dev/null || :)
if [ "${PRISM_SET}" != ".githooks" ]; then
    prism_bad "错误：core.hooksPath 没设成 .githooks（读回来是 '${PRISM_SET}'）。"
    exit 1
fi

# CRLF 的 `#!/bin/sh` 在 Linux/WSL 上是 bad interpreter，钩子连版本检查都跑不到。
PRISM_CRLF=""
for PRISM_HOOK in common.sh pre-commit pre-push check-clang-tidy.sh; do
    if [ -f ".githooks/${PRISM_HOOK}" ] && head -c 200 ".githooks/${PRISM_HOOK}" | grep -q "$(printf '\r')"; then
        PRISM_CRLF="${PRISM_CRLF} .githooks/${PRISM_HOOK}"
    fi
done
if [ -n "${PRISM_CRLF}" ]; then
    prism_bad "警告：以下钩子是 CRLF 行尾，Linux/WSL 下会 bad interpreter：${PRISM_CRLF}"
    prism_bad "      .gitattributes 已声明 .githooks/* text eol=lf，重新 checkout 即可修正。"
fi

prism_say "门禁已装上：core.hooksPath = ${PRISM_SET}（写在本仓库 .git/config，不进版本控制）"
prism_say ""
prism_say "pre-commit  staged 的 C/C++ 文件必须 clang-format 过；文件名不得是 CmakeLists.txt（vendor/** 例外）。"
prism_say "pre-push    先跑 <平台>-clang-x64-Debug 构建，再对本次推送改动的 C/C++ 文件跑 clang-tidy。"
prism_say "版本要求     clang-format / clang-tidy 必须 23.1.x，不符时钩子在动手前就报错退出（ADR 决策 6）。"
prism_say "唯一绕过场景 一次性格式批量整理，且必须在 PR 描述里说明：git commit/push --no-verify"

[ "${PRISM_FAIL}" -eq 0 ] || exit 1
exit 0
