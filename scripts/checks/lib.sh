#!/bin/sh
# 门禁公共库（#97）：本地钩子（.githooks/）与检查入口（scripts/checks/）用 `.` 载入，不单独执行。
# 契约：两侧共用同一份实现，本地 pre-commit 与 CI（format.sh ci）必须得出同一结论。
# 只写 POSIX sh，只依赖 git 与 coreutils（MSYS2 的 bash、Git for Windows 的 sh 都能跑）。

# clang 工具链版本前缀。ADR 决策 6：Windows 23.1.3 / WSL 23.1.2，只比前缀不比补丁号。
PRISM_CLANG_VERSION_PREFIX="23.1"
PRISM_PREREQUISITE_DOC="docs/markdown/项目框架.md 7.1（环境与依赖基线）"
PRISM_NO_VERIFY_RULE='--no-verify 只允许用于「一次性格式批量整理」，并且必须在 PR 描述里说明'

# 提示一律走 stderr，保证 `path=$(prism_require_tool ...)` 这类取返回值的写法不被日志污染。
prism_info()  { printf '%s\n' "[门禁] $*" >&2; }
prism_error() { printf '%s\n' "[门禁] $*" >&2; }
prism_blank() { printf '\n' >&2; }

prism_repo_root() { git rev-parse --show-toplevel 2>/dev/null; }
prism_tmpdir()    { mktemp -d "${TMPDIR:-/tmp}/prism-gate.XXXXXX"; }

# 本项目认定的 C/C++ 文件；不在此列的一律不碰（#68：不做全仓重排）。
prism_is_c_source() {
    case "$1" in
        *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx) return 0 ;;
        *) return 1 ;;
    esac
}

# prism_find_tool <工具名> -> 可执行文件路径；顺序：$CLANG_ROOT → PATH → PATH 上的 <名>-23/22/21/20
prism_find_tool() {
    _prism_name=$1
    _prism_root=${CLANG_ROOT:-}
    if [ -n "${_prism_root}" ]; then
        for _prism_cand in "${_prism_root}/${_prism_name}.exe" "${_prism_root}/${_prism_name}"; do
            if [ -x "${_prism_cand}" ]; then
                printf '%s\n' "${_prism_cand}"
                return 0
            fi
        done
    fi
    if command -v "${_prism_name}" >/dev/null 2>&1; then
        command -v "${_prism_name}"
        return 0
    fi
    for _prism_suffix in 23 22 21 20; do
        if command -v "${_prism_name}-${_prism_suffix}" >/dev/null 2>&1; then
            command -v "${_prism_name}-${_prism_suffix}"
            return 0
        fi
    done
    return 1
}

# 从 `<工具> --version` 抠出版本号，例 "clang-format version 23.1.3" -> 23.1.3
prism_tool_version() {
    "$1" --version 2>/dev/null \
        | sed -n 's/.*version[[:space:]]\{1,\}\([0-9][0-9.]*\).*/\1/p' \
        | head -n 1
}

# prism_require_tool <工具名> -> 校验存在与版本前缀；成功把路径打到 stdout，失败返回 1。
# 每个钩子的第一件事（#67 AC）：版本漂移会让同一份代码在不同机器上得出不同结论。
prism_require_tool() {
    _prism_req_name=$1

    if ! _prism_req_path=$(prism_find_tool "${_prism_req_name}"); then
        prism_error "找不到 ${_prism_req_name}，门禁无法执行。"
        prism_error "原因：门禁用它判定代码是否合规。"
        prism_error "处理：装 clang 工具链（Windows：MSYS2 mingw64，本机 23.1.3；WSL：23.1.x 的包），见 ${PRISM_PREREQUISITE_DOC}。"
        prism_error "      临时绕过：${PRISM_NO_VERIFY_RULE}。"
        return 1
    fi

    _prism_req_version=$(prism_tool_version "${_prism_req_path}")

    case "${_prism_req_version}" in
        "${PRISM_CLANG_VERSION_PREFIX}"*) ;;
        *)
            prism_error "${_prism_req_name} 版本不匹配：要求 ${PRISM_CLANG_VERSION_PREFIX}.x，实际 '${_prism_req_version:-读不出来}'（${_prism_req_path}）。"
            prism_error "原因：规则集与写它的工具版本绑定（ADR 决策 6），版本漂移会让格式检查误报或漏报。"
            prism_error "处理：换成 ${PRISM_CLANG_VERSION_PREFIX}.x 的工具链，见 ${PRISM_PREREQUISITE_DOC}。"
            prism_error "      CLANG_ROOT 当前值：'${CLANG_ROOT:-未设置}'（查找顺序：CLANG_ROOT → PATH → <工具>-23/22/21/20）。"
            prism_error "      临时绕过：${PRISM_NO_VERIFY_RULE}。"
            return 1
            ;;
    esac

    printf '%s\n' "${_prism_req_path}"
    return 0
}

# 构建前置条件（#70 AC：给可读提示而不是一堆 CMake 输出）。口径同 scripts/build.ps1|build.sh。
# $1 = windows|linux，$2 = 仓库根
prism_check_build_prerequisites() {
    _prism_pre_platform=$1
    _prism_pre_root=$2

    if ! command -v cmake >/dev/null 2>&1; then
        prism_error "前置条件不满足：PATH 上找不到 cmake（Windows 侧是独立安装的，见 ${PRISM_PREREQUISITE_DOC}）。"
        return 1
    fi

    if [ "${_prism_pre_platform}" = "windows" ]; then
        _prism_pre_missing=""
        for _prism_pre_tool in clang.exe clang++.exe ninja.exe; do
            if [ -z "${CLANG_ROOT:-}" ] || [ ! -x "${CLANG_ROOT}/${_prism_pre_tool}" ]; then
                _prism_pre_missing="${_prism_pre_missing} ${_prism_pre_tool}"
            fi
        done
        if [ -n "${_prism_pre_missing}" ]; then
            prism_error "前置条件不满足：CLANG_ROOT 未设置或无效（当前 '${CLANG_ROOT:-未设置}'），缺少：${_prism_pre_missing# }。"
            prism_error "必须是 MSYS2 mingw64 的 clang（clang64 的 libc++ 与 vcpkg 的 GCC/libstdc++ ABI 不一致）。"
            prism_error "设置方式见 ${PRISM_PREREQUISITE_DOC}。"
            return 1
        fi
    else
        if [ -z "${VCPKG_ROOT:-}" ] && [ -f "${_prism_pre_root}/scripts/wsl-env.sh" ]; then
            # shellcheck source=/dev/null
            . "${_prism_pre_root}/scripts/wsl-env.sh"
        fi
        if [ -z "${VCPKG_ROOT:-}" ] || [ ! -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]; then
            prism_error "前置条件不满足：VCPKG_ROOT 未设置或无效（当前 '${VCPKG_ROOT:-未设置}'）。"
            prism_error "WSL 侧可先 . scripts/wsl-env.sh（本次已尝试自动探测），设置方式见 ${PRISM_PREREQUISITE_DOC}。"
            return 1
        fi
    fi

    return 0
}

# prism_merge_base_with_default <远端名> -> HEAD 与默认分支的分叉点，供 clang-tidy 判定本次推送改了什么（#69）。
# 找不到任何默认分支时返回 1，退路由调用方决定。
prism_merge_base_with_default() {
    _prism_mb_remote=${1:-}
    _prism_mb_sym=""
    if [ -n "${_prism_mb_remote}" ]; then
        _prism_mb_sym=$(git symbolic-ref -q --short "refs/remotes/${_prism_mb_remote}/HEAD" 2>/dev/null || :)
    fi
    for _prism_mb_ref in \
        "refs/remotes/${_prism_mb_sym}" \
        "refs/remotes/${_prism_mb_remote}/main" \
        "refs/remotes/${_prism_mb_remote}/master" \
        "refs/remotes/origin/main" \
        "refs/remotes/origin/master" \
        "refs/heads/main" \
        "refs/heads/master"
    do
        case "${_prism_mb_ref}" in
            "refs/remotes/"|"refs/heads/"|"refs/remotes//main"|"refs/remotes//master") continue ;;
        esac
        if git rev-parse --verify -q "${_prism_mb_ref}^{commit}" >/dev/null 2>&1; then
            _prism_mb_point=$(git merge-base HEAD "${_prism_mb_ref}" 2>/dev/null || :)
            if [ -n "${_prism_mb_point}" ]; then
                printf '%s\n' "${_prism_mb_point}"
                return 0
            fi
        fi
    done
    return 1
}

# ---------- 变更范围（#97）：staged | ci | <rev> | <a>...<b> ----------
# 语法在 scripts/checks/format.sh 的 usage 里有同一份，改一处要一起改；staged 取 index 内容，其余取目标版本。
# 这几个 helper 只负责「取范围」：判定与文案留在入口里，本地钩子与 CI 于是共用同一份判定。

# prism_range_target_rev <范围参数> -> 内容取自哪个版本；staged 没有版本（回显空串）。
prism_range_target_rev() {
    case "$1" in
        staged) : ;;
        ci)
            _prism_rt_ci=$(prism_ci_range_from_env) || return 1
            printf '%s\n' "${_prism_rt_ci#*...}"
            ;;
        *...*) printf '%s\n' "${1#*...}" ;;
        *) printf '%s\n' "HEAD" ;;
    esac
}

# prism_range_source <范围参数> <路径> -> 该路径的内容所在 git 对象，形如 :foo.cpp / main:foo.cpp。
prism_range_source() {
    if [ "$1" = "staged" ]; then
        printf ':%s\n' "$2"
    else
        _prism_rsr_rev=$(prism_range_target_rev "$1") || return 1
        printf '%s:%s\n' "${_prism_rsr_rev}" "$2"
    fi
}

# prism_range_files <范围参数> <输出文件> -> 范围内的改动文件名（换行分隔）写进 <输出文件>。
# 必须带 --diff-filter=R：`git mv` 在 index 里记成 R100，漏 R 会让改名后的文件整条绕过门禁。
# -z + tr：文件名含非 ASCII 时避开 core.quotePath 转义；git 失败一律返回 1，绝不留下空列表（fail-open）。
prism_range_files() {
    _prism_rf_spec=$1
    _prism_rf_out=$2
    case "${_prism_rf_spec}" in
        '') return 1 ;;
        staged)
            git diff --cached --name-only --diff-filter=ACMR -z > "${_prism_rf_out}.z" || return 1
            ;;
        ci)
            _prism_rf_ci=$(prism_ci_range_from_env) || return 1
            git diff --name-only --diff-filter=ACMR -z "${_prism_rf_ci}" > "${_prism_rf_out}.z" || return 1
            ;;
        -*)
            # 未知选项（--help 由入口自己先处理掉）：当成用法错误，不要掉进「当 revision 用」。
            return 1
            ;;
        *...*)
            _prism_rf_a=${_prism_rf_spec%%...*}
            _prism_rf_b=${_prism_rf_spec#*...}
            if [ -z "${_prism_rf_a}" ] || [ -z "${_prism_rf_b}" ]; then
                return 1
            fi
            git diff --name-only --diff-filter=ACMR -z "${_prism_rf_a}...${_prism_rf_b}" \
                > "${_prism_rf_out}.z" || return 1
            ;;
        *)
            git diff --name-only --diff-filter=ACMR -z "${_prism_rf_spec}...HEAD" \
                > "${_prism_rf_out}.z" || return 1
            ;;
    esac
    tr '\0' '\n' < "${_prism_rf_out}.z" > "${_prism_rf_out}"
}

# prism_range_show <范围参数> <路径> <输出文件> -> 把该路径在该范围内应检查的内容写进 <输出文件>。
# stdout 是内容，stderr 交给调用方重定向；读不到返回非零（空输入会让 clang-format 以 0 退出）。
prism_range_show() {
    _prism_rsh_spec=$1
    _prism_rsh_path=$2
    _prism_rsh_out=$3
    if [ "${_prism_rsh_spec}" = "staged" ]; then
        git show ":${_prism_rsh_path}" > "${_prism_rsh_out}"
    else
        _prism_rsh_rev=$(prism_range_target_rev "${_prism_rsh_spec}") || return 1
        git show "${_prism_rsh_rev}:${_prism_rsh_path}" > "${_prism_rsh_out}"
    fi
}

# ---------- CI（GitHub Actions）环境 → 变更范围（#97）----------
# 口径必须与本地一致：CI 只负责把「这次改了什么」翻译成一个范围，翻译不出来就硬失败。
# 不静默退化成空范围 —— 空范围会让门禁「全绿」放行本该拦住的改动（fail-open）。

# prism_event_json_flat <JSON 文件> <顶层键> -> 回显该键的字符串值；取不到返回 1。
prism_event_json_flat() {
    _prism_evf_file=${1:-}
    _prism_evf_key=${2:-}
    if [ -z "${_prism_evf_file}" ] || [ ! -f "${_prism_evf_file}" ]; then
        return 1
    fi
    _prism_evf_raw=$(grep -o "\"${_prism_evf_key}\"[[:space:]]*:[[:space:]]*\"[^\"]*\"" "${_prism_evf_file}" 2>/dev/null | head -n 1)
    if [ -z "${_prism_evf_raw}" ]; then
        return 1
    fi
    printf '%s\n' "${_prism_evf_raw}" | sed 's/^"[^"]*"[[:space:]]*:[[:space:]]*"//; s/"$//'
}

# prism_event_json_child_sha <JSON 文件> <父键> -> 回显该父对象**直接子键** sha 的值；取不到返回 1。
# 坑：不能取「父键之后遇到的第一个 sha」——payload 键按字母序排，嵌套的 `repo` 排在 `sha` 之前，
# 朴素启发式在真实 payload 上直接取不到（#98）。故按嵌套层级只看深一层；用 awk 手写是为不引入 jq（#96 决策 8）。
prism_event_json_child_sha() {
    _prism_evs_file=${1:-}
    _prism_evs_key=${2:-}
    if [ -z "${_prism_evs_file}" ] || [ ! -f "${_prism_evs_file}" ]; then
        return 1
    fi
    tr -d '\r\n' < "${_prism_evs_file}" | awk -v parent="\"${_prism_evs_key}\"" -v child='"sha"' '
        { text = text $0 }
        END {
            n = length(text)
            p = index(text, parent)
            if (p == 0) { exit 1 }
            i = p + length(parent)
            # 父键的值必须是对象：跳到它的 "{"
            while (i <= n && substr(text, i, 1) != "{") { i++ }
            if (i > n) { exit 1 }
            depth = 0
            while (i <= n) {
                ch = substr(text, i, 1)
                if (ch == "\"") {
                    # 读一个字符串字面量（含转义）
                    j = i + 1
                    lit = ""
                    while (j <= n) {
                        c2 = substr(text, j, 1)
                        if (c2 == "\\") { lit = lit substr(text, j, 2); j += 2; continue }
                        if (c2 == "\"") { break }
                        lit = lit c2
                        j++
                    }
                    next_i = j + 1
                    # 深度 1 = 父对象的直接子键
                    if (depth == 1 && ("\"" lit "\"") == child) {
                        k = next_i
                        while (k <= n && substr(text, k, 1) != ":") { k++ }
                        k++
                        while (k <= n && substr(text, k, 1) ~ /[ \t\r\n]/) { k++ }
                        if (substr(text, k, 1) == "\"") {
                            m = k + 1
                            val = ""
                            while (m <= n) {
                                c3 = substr(text, m, 1)
                                if (c3 == "\\") { val = val substr(text, m, 2); m += 2; continue }
                                if (c3 == "\"") { break }
                                val = val c3
                                m++
                            }
                            print val
                            exit 0
                        }
                    }
                    i = next_i
                    continue
                }
                if (ch == "{") { depth++ }
                else if (ch == "}") { depth--; if (depth <= 0) { exit 1 } }
                i++
            }
            exit 1
        }
    '
}

# prism_ci_range_from_env -> 回显 <base>...<head>；环境给不出可信范围时返回 1 并说明缺什么。
prism_ci_range_from_env() {
    _prism_ci_event=${GITHUB_EVENT_NAME:-}
    _prism_ci_head=${GITHUB_SHA:-}
    _prism_ci_base=${GITHUB_BASE_SHA:-}
    _prism_ci_path=${GITHUB_EVENT_PATH:-}

    case "${_prism_ci_event}" in
        pull_request|pull_request_target)
            # base 优先用 GitHub 直接给的 GITHUB_BASE_SHA，其次读事件的 .pull_request.base.sha。
            if [ -z "${_prism_ci_base}" ]; then
                _prism_ci_base=$(prism_event_json_child_sha "${_prism_ci_path}" base || :)
            fi
            # head 优先用 .pull_request.head.sha（PR 的改动侧），取不到才退回 GITHUB_SHA。
            _prism_ci_head=$(prism_event_json_child_sha "${_prism_ci_path}" head || :)
            if [ -z "${_prism_ci_head}" ]; then
                _prism_ci_head=${GITHUB_SHA:-}
            fi
            ;;
        push)
            _prism_ci_base=$(prism_event_json_flat "${_prism_ci_path}" before || :)
            if [ -z "${_prism_ci_base}" ]; then
                _prism_ci_base=${GITHUB_BASE_SHA:-}
            fi
            _prism_ci_head=${GITHUB_SHA:-}
            # push 的 .before 全零 = 新建分支 / 首次推送：没有可信的 base，不许悄悄放大范围。
            case "${_prism_ci_base}" in
                *[!0]*) ;;
                *)
                    prism_error "CI 范围解析失败：事件里的 .before 为空或全零（新建分支的首次推送？），没有可信的 base。"
                    prism_error "处理：显式给 GITHUB_BASE_SHA，或改用 <a>...<b> 直接指定范围。"
                    return 1
                    ;;
            esac
            ;;
        '')
            prism_error "CI 范围解析失败：没有 GITHUB_EVENT_NAME，无法判定这次是 pull_request 还是 push。"
            prism_error "处理：在 GitHub Actions 里跑（这些变量由 runner 注入），或改用 staged / <rev> / <a>...<b>。"
            return 1
            ;;
        *)
            prism_error "CI 范围解析失败：不支持的事件类型 '${_prism_ci_event}'。"
            prism_error "支持的事件（GITHUB_EVENT_NAME）：pull_request、pull_request_target、push。"
            return 1
            ;;
    esac

    if [ -z "${_prism_ci_base}" ] || [ -z "${_prism_ci_head}" ]; then
        prism_error "CI 范围解析失败：base='${_prism_ci_base:-未取到}'，head='${_prism_ci_head:-未取到}'。"
        prism_error "处理：确认 GITHUB_EVENT_PATH 指向真实的事件 JSON，且 GITHUB_SHA / GITHUB_BASE_SHA 可用。"
        return 1
    fi

    printf '%s...%s\n' "${_prism_ci_base}" "${_prism_ci_head}"
}
