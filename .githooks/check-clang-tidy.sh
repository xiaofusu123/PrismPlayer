#!/bin/sh
# clang-tidy 检查（#69）：只查本次推送相对默认分支改动的 C/C++ 文件。
# 用法：check-clang-tidy.sh <编译数据库所在目录> [基线 ref]；基线为空串时按全仓查。
# 退出码：0 = 通过（含「没有 C/C++ 改动」）；1 = 有违规，或检查无法完成。
#
# 只查改动文件：全量 clang-tidy 几十秒起步，慢到会被 --no-verify 绕过；三条规则基线已清零，
# 本次改动引入的违规就是目标。
# 判定不看退出码：数据库加载失败时 clang-tidy 会打印错误却以 0 退出，只看退出码等于放行。

set -u

PRISM_HOOK_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=.githooks/common.sh
. "${PRISM_HOOK_DIR}/common.sh"

PRISM_DB_DIR=${1:-}
PRISM_BASE=${2:-}

if [ -z "${PRISM_DB_DIR}" ]; then
    prism_error "用法：check-clang-tidy.sh <编译数据库所在目录> [基线 ref]"
    exit 1
fi

if ! PRISM_TIDY=$(prism_require_tool clang-tidy); then
    exit 1
fi

PRISM_ROOT=$(prism_repo_root) || {
    prism_error "不在 Git 仓库里，无法执行 clang-tidy 检查。"
    exit 1
}
cd "${PRISM_ROOT}" || exit 1

PRISM_DB="${PRISM_DB_DIR}/compile_commands.json"

PRISM_TMP=$(prism_tmpdir) || {
    prism_error "创建临时目录失败，无法执行检查。"
    exit 1
}
trap 'rm -rf "${PRISM_TMP}"' EXIT HUP INT TERM

# -z + tr：避开 core.quotePath 对非 ASCII 文件名的转义。
if [ -n "${PRISM_BASE}" ]; then
    git diff --name-only --diff-filter=ACMR -z "${PRISM_BASE}...HEAD" | tr '\0' '\n' > "${PRISM_TMP}/raw"
else
    git ls-files -z | tr '\0' '\n' > "${PRISM_TMP}/raw"
fi

# ---- 本次改动的 C/C++ 文件 ----
: > "${PRISM_TMP}/changed_c"
while IFS= read -r PRISM_F; do
    [ -n "${PRISM_F}" ] || continue
    prism_is_c_source "${PRISM_F}" || continue
    # 工作区里已不存在的（删除/改名）没法分析。
    [ -f "${PRISM_F}" ] || continue
    printf '%s\n' "${PRISM_F}" >> "${PRISM_TMP}/changed_c"
done < "${PRISM_TMP}/raw"

if [ ! -s "${PRISM_TMP}/changed_c" ]; then
    prism_info "本次推送没有 C/C++ 改动，跳过 clang-tidy（不产生额外耗时）。"
    exit 0
fi
PRISM_CHANGED_COUNT=$(wc -l < "${PRISM_TMP}/changed_c" | tr -d ' ')
prism_info "clang-tidy 检查范围：本次改动的 ${PRISM_CHANGED_COUNT} 个 C/C++ 文件"

# 数据库只在确实要查时才要求：没有 C/C++ 改动就不该因为缺数据库而拦人（#69 AC）。
if [ ! -f "${PRISM_DB}" ]; then
    prism_error "找不到编译数据库：${PRISM_DB}"
    prism_error "原因：clang-tidy 要有编译参数（include 路径、宏、语言标准）才能正确分析，缺了它结论不可信。"
    prism_error "处理：跑一次 cmake --preset <平台>-clang-x64-Debug 生成它；"
    prism_error "      本仓库已在 CMakePresets.json 里设 CMAKE_EXPORT_COMPILE_COMMANDS=ON，配置阶段就会产出。"
    exit 1
fi

# ---- 仓库里所有跟踪的 C/C++ 文件（用于追溯 include 链）----
git ls-files -z | tr '\0' '\n' > "${PRISM_TMP}/all_tracked"
: > "${PRISM_TMP}/all_c"
while IFS= read -r PRISM_F; do
    prism_is_c_source "${PRISM_F}" || continue
    [ -f "${PRISM_F}" ] || continue
    printf '%s\n' "${PRISM_F}" >> "${PRISM_TMP}/all_c"
done < "${PRISM_TMP}/all_tracked"

# 基名 → 路径 的表：把头文件里写的 include 名字解析回仓库内的文件。
: > "${PRISM_TMP}/all_c_base"
while IFS= read -r PRISM_F; do
    printf '%s\t%s\n' "$(basename -- "${PRISM_F}")" "${PRISM_F}" >> "${PRISM_TMP}/all_c_base"
done < "${PRISM_TMP}/all_c"

# ---- 编译数据库里的源文件（绝对路径、正斜杠）----
grep -o '"file"[[:space:]]*:[[:space:]]*"[^"]*"' "${PRISM_DB}" \
    | sed -e 's/^"file"[[:space:]]*:[[:space:]]*"//' -e 's/"$//' > "${PRISM_TMP}/db_sources"

if [ ! -s "${PRISM_TMP}/db_sources" ]; then
    prism_error "编译数据库里解析不出任何源文件条目：${PRISM_DB}"
    prism_error "处理：删掉构建目录重新配置（scripts/build.ps1 1 --clean）后重试。"
    exit 1
fi

# 仓库内相对路径 f 是否为一个编译单元：数据库存绝对路径，故用带斜杠的后缀匹配。
prism_is_tu() {
    grep -q -i -F -- "/$1" "${PRISM_TMP}/db_sources"
}

: > "${PRISM_TMP}/tus"
while IFS= read -r PRISM_F; do
    if prism_is_tu "${PRISM_F}"; then
        printf '%s\n' "${PRISM_F}" >> "${PRISM_TMP}/tus"
    fi
done < "${PRISM_TMP}/changed_c"

# ---- 向上找「谁 include 了改动的头文件」，最多 3 层（实测本仓库深度为 2），得到要分析的编译单元 ----
cp "${PRISM_TMP}/changed_c" "${PRISM_TMP}/frontier"
: > "${PRISM_TMP}/walked"
PRISM_ROUND=0
while [ "${PRISM_ROUND}" -lt 3 ] && [ -s "${PRISM_TMP}/frontier" ]; do
    PRISM_ROUND=$((PRISM_ROUND + 1))
    : > "${PRISM_TMP}/found"
    while IFS= read -r PRISM_F; do
        PRISM_BASE_NAME=$(basename -- "${PRISM_F}")
        # 固定字符串匹配（只按基名，本项目无重名头文件）：不用正则，省掉转义，
        # 也避开不同 sed 对 `\\&` 的处理差异。
        # shellcheck disable=SC2046
        grep -l -F -e "#include \"${PRISM_BASE_NAME}\"" -e "#include <${PRISM_BASE_NAME}>" \
            $(cat "${PRISM_TMP}/all_c") 2>/dev/null >> "${PRISM_TMP}/found"
    done < "${PRISM_TMP}/frontier"
    sort -u -o "${PRISM_TMP}/found" "${PRISM_TMP}/found"

    # 本轮的 frontier 与刚找到的 includer 都算「走到过」：最后一轮找到的 includer
    # 不会再作为下一轮 frontier，漏掉它就会漏掉跨 3 层头文件的编译单元。
    cat "${PRISM_TMP}/frontier" >> "${PRISM_TMP}/walked"
    sort -u "${PRISM_TMP}/walked" > "${PRISM_TMP}/walked.sorted"
    comm -23 "${PRISM_TMP}/found" "${PRISM_TMP}/walked.sorted" > "${PRISM_TMP}/frontier.next"
    cat "${PRISM_TMP}/found" >> "${PRISM_TMP}/walked"
    mv "${PRISM_TMP}/frontier.next" "${PRISM_TMP}/frontier"
done

# 走到过的文件里，编译数据库认识的才算可分析的编译单元。
: > "${PRISM_TMP}/tus"
sort -u "${PRISM_TMP}/walked" > "${PRISM_TMP}/walked.sorted"
while IFS= read -r PRISM_F; do
    [ -n "${PRISM_F}" ] || continue
    if prism_is_tu "${PRISM_F}"; then
        printf '%s\n' "${PRISM_F}" >> "${PRISM_TMP}/tus"
    fi
done < "${PRISM_TMP}/walked"
sort -u -o "${PRISM_TMP}/tus" "${PRISM_TMP}/tus"

# ---- 覆盖判定：从这些编译单元沿 include 链往下走（最多 3 层），落在链上的改动文件才算查过 ----
# 不能按「有人 include 我」反着判：include 它的可能是个根本不参与编译的半成品。
sort -u "${PRISM_TMP}/tus" > "${PRISM_TMP}/reached_all"
PRISM_ROUND=0
while [ "${PRISM_ROUND}" -lt 3 ]; do
    PRISM_ROUND=$((PRISM_ROUND + 1))
    : > "${PRISM_TMP}/inc_names"
    while IFS= read -r PRISM_F; do
        [ -f "${PRISM_F}" ] || continue
        sed -n 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]\([^">]*\)[">].*/\1/p' "${PRISM_F}" \
            | sed 's|.*/||' >> "${PRISM_TMP}/inc_names"
    done < "${PRISM_TMP}/reached_all"
    sort -u -o "${PRISM_TMP}/inc_names" "${PRISM_TMP}/inc_names"

    awk -F'\t' 'NR==FNR { want[$1]=1; next } ($1 in want) { print $2 }' \
        "${PRISM_TMP}/inc_names" "${PRISM_TMP}/all_c_base" | sort -u > "${PRISM_TMP}/found"
    sort -u "${PRISM_TMP}/reached_all" "${PRISM_TMP}/found" > "${PRISM_TMP}/reached_all.next"
    mv "${PRISM_TMP}/reached_all.next" "${PRISM_TMP}/reached_all"
done

# ---- 明确列出覆盖不到的改动文件，不假装查过 ----
sort -u "${PRISM_TMP}/changed_c" > "${PRISM_TMP}/changed.sorted"
sort -u "${PRISM_TMP}/reached_all" > "${PRISM_TMP}/reached.sorted"
comm -23 "${PRISM_TMP}/changed.sorted" "${PRISM_TMP}/reached.sorted" > "${PRISM_TMP}/uncovered"

if [ -s "${PRISM_TMP}/uncovered" ]; then
    prism_info "以下改动文件没有任何编译单元引用（3 层内），clang-tidy 覆盖不到："
    sed 's/^/[门禁]   /' "${PRISM_TMP}/uncovered" >&2
fi

# ---- header-filter：只报改动文件里的头文件诊断；超过 30 个就不筛，靠后处理与判定兜底 ----
PRISM_HF=""
if [ "${PRISM_CHANGED_COUNT}" -le 30 ]; then
    while IFS= read -r PRISM_F; do
        # 只用 [.] 转义点号：不引反斜杠，避开不同 sed 对 `\\&` 的处理差异。
        PRISM_ESC=$(printf '%s\n' "${PRISM_F}" | sed 's/[.]/[.]/g')
        if [ -z "${PRISM_HF}" ]; then PRISM_HF="${PRISM_ESC}"; else PRISM_HF="${PRISM_HF}|${PRISM_ESC}"; fi
    done < "${PRISM_TMP}/changed_c"
else
    PRISM_HF="."
fi

# ---- 跑检查 ----
: > "${PRISM_TMP}/tidy.log"
: > "${PRISM_TMP}/ran"
PRISM_TIDY_STATUS=0
if [ ! -s "${PRISM_TMP}/tus" ]; then
    prism_info "没有可分析的编译单元，clang-tidy 这一步无事可做。"
else
    while IFS= read -r PRISM_TU; do
        prism_info "运行 clang-tidy：${PRISM_TU}"
        if ! "${PRISM_TIDY}" -p "${PRISM_DB_DIR}" --quiet --header-filter="${PRISM_HF}" "${PRISM_TU}" \
            >> "${PRISM_TMP}/tidy.log" 2>&1; then
            PRISM_TIDY_STATUS=1
        fi
        printf '%s\n' "${PRISM_TU}" >> "${PRISM_TMP}/ran"
    done < "${PRISM_TMP}/tus"
fi

if grep -q -e "Error while trying to load a compilation database" \
            -e "Running without flags" "${PRISM_TMP}/tidy.log"; then
    prism_blank
    prism_error "clang-tidy 没能加载编译数据库，结论不可信（这种失败以退出码 0 结束，必须显式拦）。"
    prism_error "数据库：${PRISM_DB}"
    prism_error "处理：删掉构建目录重新配置（scripts/build.ps1 1 --clean）后再 push。"
    exit 1
fi

PRISM_DIAG_COUNT=$(grep -c -E ':[0-9][0-9]*:[0-9][0-9]*: (error|warning):' "${PRISM_TMP}/tidy.log" || :)
PRISM_ERR_COUNT=$(grep -c -E '(error|warning):' "${PRISM_TMP}/tidy.log" || :)

if [ "${PRISM_DIAG_COUNT}" -ne 0 ]; then
    grep -E ':[0-9][0-9]*:[0-9][0-9]*: (error|warning):' "${PRISM_TMP}/tidy.log" > "${PRISM_TMP}/diags"
    prism_blank
    prism_error "clang-tidy 未通过：${PRISM_DIAG_COUNT} 处诊断（三条规则都是致命错误）。"
    while IFS= read -r PRISM_LINE; do
        [ -n "${PRISM_LINE}" ] || continue
        prism_error "  ${PRISM_LINE}"
        # 标注这条诊断落在哪个改动文件上。
        PRISM_PATH=$(printf '%s\n' "${PRISM_LINE}" | sed -n 's/^\(.*\):[0-9][0-9]*:[0-9][0-9]*: .*$/\1/p')
        [ -n "${PRISM_PATH}" ] || continue
        while IFS= read -r PRISM_F; do
            case "${PRISM_PATH}" in
                *"/${PRISM_F}")
                    prism_error "      ↳ 落在本次改动的文件上：${PRISM_F}"
                    ;;
            esac
        done < "${PRISM_TMP}/changed_c"
    done < "${PRISM_TMP}/diags"
    prism_blank
    prism_error "处理：按 docs/markdown/代码规范.md 对应章节改代码（§1.1-1.3 命名、§4.6 虚析构、§5.1 函数长度）。"
    prism_error "      确实改不了的只允许就地 NOLINTNEXTLINE 并注明原因（ADR 决策 13），不允许关规则或降级。"
    exit 1
fi

if [ "${PRISM_ERR_COUNT}" -ne 0 ] || [ "${PRISM_TIDY_STATUS}" -ne 0 ]; then
    prism_blank
    prism_error "clang-tidy 报错但诊断不是「文件:行:列: kind:」格式，无法定位。原始输出："
    tail -n 40 "${PRISM_TMP}/tidy.log" | sed 's/^/[门禁]   /' >&2
    prism_error "处理：本地手动跑一次上面的命令复现，再判断是代码问题还是工具链问题。"
    exit 1
fi

prism_info "clang-tidy 通过：${PRISM_CHANGED_COUNT} 个改动文件，分析了 $(wc -l < "${PRISM_TMP}/ran" | tr -d ' ') 个编译单元，0 处诊断。"
exit 0
