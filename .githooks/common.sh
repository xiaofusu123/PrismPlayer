#!/bin/sh
# 门禁公共库：由 .githooks/pre-commit 与 pre-push 用 `.` 载入，不单独执行。
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

# 取本地 HEAD 与默认分支的分叉点，供 clang-tidy 判定「本次推送改了什么」（#69）。
# $1 = 远端名；找不到任何默认分支时返回 1，由调用方决定退路。
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
