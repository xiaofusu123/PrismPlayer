#!/bin/sh
# 格式与文件名检查入口（#97）：把「哪些文件要过检查、不合规时说什么」从 Git 钩子里抽出来。
# 本地 pre-commit（staged）与 CI（ci）跑的是这一份，得出同一个结论，不各写一套。
#
# 用法：sh scripts/checks/format.sh <变更范围>
#   staged      暂存内容（index）：文件取自 git diff --cached，内容取自 git show :<路径>
#   ci          GitHub Actions：从 GITHUB_EVENT_NAME / GITHUB_SHA / GITHUB_EVENT_PATH 解析范围
#   <rev>       与默认分支的分叉点：文件取自 git diff <rev>...HEAD，内容取自 HEAD:<路径>
#   <a>...<b>   任意范围：文件取自 git diff <a>...<b>，内容取自 <b>:<路径>
#
# 判定：① 文件名不得是 CmakeLists.txt（vendor/** 只豁免这一条，#39 决策 29）；
#       ② 范围内的 C/C++ 文件必须已经 clang-format 过（--dry-run --Werror）。
# 退出码：0 = 通过（含空变更范围）；1 = 有违规，或检查无法完成（git 失败绝不静默放行）。

set -u

PRISM_ENTRY_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=scripts/checks/lib.sh
. "${PRISM_ENTRY_DIR}/lib.sh"

prism_usage() {
    cat <<'EOF'
用法：sh scripts/checks/format.sh <变更范围>

把「哪些文件要过格式/文件名检查、不合规时说什么」判定一遍，退出码与报错文案同 pre-commit 门禁。

变更范围（四选一）：
  staged       暂存内容（index）：文件取自 git diff --cached，内容取自 git show :<路径>
  ci           GitHub Actions：由 GITHUB_EVENT_NAME / GITHUB_SHA / GITHUB_EVENT_PATH 解析出 <base>...<head>
  <rev>        与默认分支的分叉点，如 main：文件取自 git diff <rev>...HEAD，内容取自 HEAD:<路径>
  <a>...<b>    任意范围：文件取自 git diff <a>...<b>，内容取自 <b>:<路径>

判定：
  ① 文件名不得是 CmakeLists.txt（正确写法 CMakeLists.txt）；vendor/** 只豁免这一条；
  ② 范围内的 C/C++ 文件（.c/.cc/.cpp/.cxx/.h/.hh/.hpp/.hxx）必须过 clang-format --dry-run --Werror。

退出码：0 = 通过（含空变更范围）；1 = 有违规，或检查无法完成。

例：sh scripts/checks/format.sh staged        # 与 pre-commit 同一份判定
    sh scripts/checks/format.sh main          # 相对默认分支的分叉点
    sh scripts/checks/format.sh main...HEAD   # 任意范围
    sh scripts/checks/format.sh ci            # CI（GitHub Actions）
EOF
}

# 第一件事：校验工具版本前缀（#67 AC）。版本漂移会让同一份代码在不同机器上得出不同结论。
if ! PRISM_CLANG_FORMAT=$(prism_require_tool clang-format); then
    exit 1
fi

if [ "$#" -gt 1 ]; then
    prism_error "参数太多：只接受一个变更范围。"
    prism_usage >&2
    exit 1
fi
PRISM_SPEC=${1:-}

case "${PRISM_SPEC}" in
    -h|--help)
        prism_usage
        exit 0
        ;;
    '')
        prism_error "缺少变更范围参数。"
        prism_usage >&2
        exit 1
        ;;
    -*)
        prism_error "不认识的参数：${PRISM_SPEC}"
        prism_usage >&2
        exit 1
        ;;
esac

PRISM_ROOT=$(prism_repo_root) || {
    prism_error "不在 Git 仓库里，无法执行格式检查（拒绝放行，避免静默通过）。"
    exit 1
}
cd "${PRISM_ROOT}" || exit 1

PRISM_TMP=$(prism_tmpdir) || {
    prism_error "创建临时目录失败，无法执行检查。"
    exit 1
}
trap 'rm -rf "${PRISM_TMP}"' EXIT HUP INT TERM

if ! prism_range_files "${PRISM_SPEC}" "${PRISM_TMP}/range"; then
    prism_error "读取变更范围失败（${PRISM_SPEC}），无法判定，拒绝放行。"
    prism_error "范围写法：staged | ci | <rev> | <a>...<b>，见 --help。"
    exit 1
fi

# 只有一处文案分叉：staged 模式必须与抽入口之前的 pre-commit 逐字一致，其余范围用「改动的」措辞。
# 判定的代码路径始终是同一条，这里只换词。
if [ "${PRISM_SPEC}" = "staged" ]; then
    PRISM_PASS_MODE=" staged "
    PRISM_REJECT_SUBJECT=" staged 文件"
    PRISM_REJECT_WORD="提交"
    PRISM_SOURCE_WORD="index"
    PRISM_TAIL_FIX="改完只 stage 这些文件再提交即可。"
else
    PRISM_PASS_MODE="改动的 "
    PRISM_REJECT_SUBJECT="改动的文件"
    PRISM_REJECT_WORD="检查"
    PRISM_SOURCE_WORD=$(prism_range_target_rev "${PRISM_SPEC}") || PRISM_SOURCE_WORD="HEAD"
    PRISM_TAIL_FIX="改完这些文件后重新跑一次本入口即可。"
fi

# 空范围不是错误：git 成功但范围内没有改动文件（比如只改文档的提交）就是无事可做。
if [ ! -s "${PRISM_TMP}/range" ]; then
    prism_info "通过：变更范围（${PRISM_SPEC}）为空，没有需要检查的文件。"
    exit 0
fi

: > "${PRISM_TMP}/bad_names"
: > "${PRISM_TMP}/bad_format"
: > "${PRISM_TMP}/format_log"

PRISM_CHECKED=0

# 用重定向而不是管道喂循环，避免整个循环掉进子 shell 导致计数器丢失。
while IFS= read -r PRISM_FILE; do
    [ -n "${PRISM_FILE}" ] || continue

    # vendor/ 不参与构建，那 3 个 CmakeLists.txt 永久保留（#39 决策 29）：只豁免文件名检查，
    # 格式检查照做 —— vendor 里的 C/C++ 也是要提交的代码（AGENTS.md「只做格式检查」）。
    if [ "$(basename -- "${PRISM_FILE}")" = "CmakeLists.txt" ]; then
        case "${PRISM_FILE}" in
            vendor/*) ;;
            *) printf '%s\n' "${PRISM_FILE}" >> "${PRISM_TMP}/bad_names" ;;
        esac
    fi

    prism_is_c_source "${PRISM_FILE}" || continue

    PRISM_CHECKED=$((PRISM_CHECKED + 1))

    PRISM_SOURCE=$(prism_range_source "${PRISM_SPEC}" "${PRISM_FILE}")
    # cwd 已是仓库根，--assume-filename 用仓库内相对路径，clang-format 靠它找 .clang-format 并判语言。
    # 先落到文件再喂进去：命令替换会吃掉结尾换行，那本身就会被 clang-format 判成不合规。
    if ! prism_range_show "${PRISM_SPEC}" "${PRISM_FILE}" "${PRISM_TMP}/blob" 2>"${PRISM_TMP}/show_err"; then
        # 读不到内容就判不合规：空输入会让 clang-format 以 0 退出，静默放行是最坏的失败方式。
        printf '%s\n' "${PRISM_FILE}" >> "${PRISM_TMP}/bad_format"
        {
            printf '%s\n' "----- ${PRISM_FILE}"
            printf '%s\n' "无法从 ${PRISM_SOURCE_WORD} 读取该文件（git show ${PRISM_SOURCE} 失败）："
            cat "${PRISM_TMP}/show_err"
        } >> "${PRISM_TMP}/format_log"
        continue
    fi

    PRISM_FMT_OUT=$("${PRISM_CLANG_FORMAT}" --dry-run --Werror --assume-filename="${PRISM_FILE}" - \
        < "${PRISM_TMP}/blob" 2>&1)
    PRISM_FMT_STATUS=$?

    if [ "${PRISM_FMT_STATUS}" -ne 0 ]; then
        printf '%s\n' "${PRISM_FILE}" >> "${PRISM_TMP}/bad_format"
        {
            printf '%s\n' "----- ${PRISM_FILE}"
            printf '%s\n' "${PRISM_FMT_OUT}"
        } >> "${PRISM_TMP}/format_log"
    fi
done < "${PRISM_TMP}/range"

PRISM_NAME_COUNT=$(wc -l < "${PRISM_TMP}/bad_names" | tr -d ' ')
PRISM_FORMAT_COUNT=$(wc -l < "${PRISM_TMP}/bad_format" | tr -d ' ')

if [ "${PRISM_NAME_COUNT}" -eq 0 ] && [ "${PRISM_FORMAT_COUNT}" -eq 0 ]; then
    prism_info "通过：${PRISM_CHECKED} 个${PRISM_PASS_MODE}C/C++ 文件格式合规，没有 CmakeLists.txt。"
    exit 0
fi

prism_blank
prism_error "${PRISM_REJECT_WORD}被拒绝：下面这些${PRISM_REJECT_SUBJECT}没有通过门禁。"

if [ "${PRISM_NAME_COUNT}" -ne 0 ]; then
    prism_blank
    prism_error "① 文件名错误（正确写法是 CMakeLists.txt：大写 CMake、小写 lists）："
    while IFS= read -r PRISM_BAD; do
        [ -n "${PRISM_BAD}" ] || continue
        prism_error "     ${PRISM_BAD}"
        prism_error "     修复：git mv -f -- \"${PRISM_BAD}\" \"$(dirname -- "${PRISM_BAD}")/CMakeLists.txt\""
        prism_error "     Windows 不区分大小写，本地能过；Linux 上 CMake 找不到这个文件。"
    done < "${PRISM_TMP}/bad_names"
fi

if [ "${PRISM_FORMAT_COUNT}" -ne 0 ]; then
    prism_blank
    prism_error "② 格式不合规（clang-format --dry-run --Werror 未通过）："
    while IFS= read -r PRISM_BAD; do
        [ -n "${PRISM_BAD}" ] || continue
        prism_error "     ${PRISM_BAD}"
        prism_error "     修复：\"${PRISM_CLANG_FORMAT}\" -i -- \"${PRISM_BAD}\""
    done < "${PRISM_TMP}/bad_format"
    prism_blank
    # shellcheck disable=SC2013
    PRISM_FIX_LIST=$(tr '\n' ' ' < "${PRISM_TMP}/bad_format")
    prism_error "     一次修完：\"${PRISM_CLANG_FORMAT}\" -i -- ${PRISM_FIX_LIST}"
    prism_blank
    prism_error "     clang-format 判定详情："
    sed 's/^/       /' "${PRISM_TMP}/format_log" >&2
fi

prism_blank
prism_error "${PRISM_TAIL_FIX}"
prism_error "要绕过（仅限一次性格式批量整理，且须在 PR 描述里说明）：git commit --no-verify"
exit 1
