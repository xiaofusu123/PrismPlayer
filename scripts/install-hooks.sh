#!/bin/sh
# 一条命令装上门禁（#67）：把 core.hooksPath 指向版本控制里的 .githooks/。
# 用 hooksPath 而不是往 .git/hooks/ 拷贝：拷贝出来的钩子不进版本控制，别人 clone 完是裸奔的。
# 用法：sh scripts/install-hooks.sh　（Windows 请用 scripts/install-hooks.ps1）

set -u

PRISM_INSTALL_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "${PRISM_INSTALL_ROOT}" || exit 1

PRISM_FAIL=0
prism_say()  { printf '%s\n' "$*"; }
prism_bad()  { printf '%s\n' "$*" >&2; PRISM_FAIL=1; }
prism_warn() { printf '%s\n' "$*" >&2; }

# 钩子只列一次：存在性检查、执行位、CRLF 自检都从它派生。
PRISM_HOOKS="pre-commit pre-push"
# 检查入口（#97 / #100）：本地钩子与 CI 的判定都来自这里，也要有 exec 位。
PRISM_CHECK_SCRIPTS="scripts/checks/lib.sh scripts/checks/format.sh scripts/checks/tidy.sh"

PRISM_CHMOD_TARGETS=""
for PRISM_HOOK in ${PRISM_HOOKS}; do
    if [ ! -f ".githooks/${PRISM_HOOK}" ]; then
        prism_bad "错误：找不到 .githooks/${PRISM_HOOK}（当前目录 ${PRISM_INSTALL_ROOT} 可能不是仓库根）。"
    fi
    PRISM_CHMOD_TARGETS="${PRISM_CHMOD_TARGETS} .githooks/${PRISM_HOOK}"
done
for PRISM_CHECK_SCRIPT in ${PRISM_CHECK_SCRIPTS}; do
    if [ ! -f "${PRISM_CHECK_SCRIPT}" ]; then
        prism_bad "错误：找不到 ${PRISM_CHECK_SCRIPT}（当前目录 ${PRISM_INSTALL_ROOT} 可能不是仓库根）。"
    fi
    PRISM_CHMOD_TARGETS="${PRISM_CHMOD_TARGETS} ${PRISM_CHECK_SCRIPT}"
done
[ "${PRISM_FAIL}" -eq 0 ] || exit 1

# 执行位：WSL/Linux 上缺了 git 会静默忽略钩子；Windows 上 chmod 是空操作。
# shellcheck disable=SC2086
chmod +x ${PRISM_CHMOD_TARGETS} 2>/dev/null || :

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
# 只警告不失败：hooksPath 确实装上了，重新 checkout 就能修正（.ps1 侧行为一致）。
PRISM_CRLF=""
for PRISM_HOOK in ${PRISM_HOOKS}; do
    if [ -f ".githooks/${PRISM_HOOK}" ] && head -c 200 ".githooks/${PRISM_HOOK}" | grep -q "$(printf '\r')"; then
        PRISM_CRLF="${PRISM_CRLF} .githooks/${PRISM_HOOK}"
    fi
done
if [ -n "${PRISM_CRLF}" ]; then
    prism_warn "警告：以下钩子是 CRLF 行尾，Linux/WSL 下会 bad interpreter：${PRISM_CRLF}"
    prism_warn "      .gitattributes 已声明 .githooks/* text eol=lf，重新 checkout 即可修正。"
fi

# 版本要求从检查入口的公共库读，不在安装脚本里再写一遍（避免两处各说一套）。
# shellcheck source=scripts/checks/lib.sh
. scripts/checks/lib.sh

prism_say "门禁已装上：core.hooksPath = ${PRISM_SET}（写在本仓库 .git/config，不进版本控制）"
prism_say ""
prism_say "pre-commit  staged 的 C/C++ 文件必须 clang-format 过；文件名不得是 CmakeLists.txt（vendor/** 只豁免这一条）。"
prism_say "pre-push    先跑 <平台>-clang-x64-Debug 构建，再对本次推送改动的 C/C++ 文件跑 clang-tidy。"
prism_say "检查入口     scripts/checks/format.sh <staged|ci|<rev>|<a>...<b>> 一条命令打印与钩子同源的判定结果。"
prism_say "版本要求     clang-format / clang-tidy 必须 ${PRISM_CLANG_VERSION_PREFIX}.x，不符时钩子在动手前就报错退出（ADR 决策 6）。"
prism_say "唯一绕过场景 一次性格式批量整理，且必须在 PR 描述里说明：git commit/push --no-verify"

exit 0
