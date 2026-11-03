#!/bin/sh
# 门禁自测共用：断言、临时仓库夹具、计数与收尾。
# 由 scripts/tests/{hooks,checks}.sh source；调用前须设置 PRISM_TEST_ROOT 与 PRISM_CASE_ROOT。

set -u

PRISM_RUN_SH=$(command -v sh) || PRISM_RUN_SH=/bin/sh
PRISM_PASS=0
PRISM_FAIL=0
PRISM_CASE_SEQ=0

# ---------- 断言 ----------
ok()  { PRISM_PASS=$((PRISM_PASS + 1)); printf '    [PASS] %s\n' "$1"; }
bad() {
    PRISM_FAIL=$((PRISM_FAIL + 1))
    printf '    [FAIL] %s\n' "$1"
    printf '          退出码=%s\n' "${PRISM_STATUS}"
    printf '          输出：\n'
    printf '%s\n' "${PRISM_OUT}" | sed 's/^/            | /'
}

expect_status() {
    if [ "${PRISM_STATUS}" -eq "$1" ]; then ok "$2"; else bad "$2（期望退出码 $1）"; fi
}

expect_status_nonzero() {
    if [ "${PRISM_STATUS}" -ne 0 ]; then ok "$1"; else bad "$1（期望非零退出码）"; fi
}

expect_out_has() {
    case "${PRISM_OUT}" in
        *"$1"*) ok "$2" ;;
        *) bad "$2（输出里没有 '$1'）" ;;
    esac
}

expect_out_lacks() {
    case "${PRISM_OUT}" in
        *"$1"*) bad "$2（输出里不该出现 '$1'）" ;;
        *) ok "$2" ;;
    esac
}

# 逐字比对，用来断言 pre-commit 与入口是同一份判定。
expect_out_equals() {
    if [ "${PRISM_OUT}" = "$1" ]; then
        ok "$2"
    else
        bad "$2（输出与预期不一致）"
        printf '          预期：\n'
        printf '%s\n' "$1" | sed 's/^/            | /'
    fi
}

# run_in <目录> <命令...>：切 cwd 执行，合并捕获 stdout+stderr，stdin 接 /dev/null 防卡住。
run_in() {
    _prism_dir=$1
    shift
    PRISM_OUT=$(cd "${_prism_dir}" && "$@" 2>&1 < /dev/null)
    PRISM_STATUS=$?
}

# ---------- 夹具 ----------
# 夹具放仓库 build/ 下（已 gitignore）；别用 /tmp —— Git Bash 的 /tmp 与 git.exe 认的不是同一个。
new_fixture() {
    PRISM_CASE_SEQ=$((PRISM_CASE_SEQ + 1))
    PRISM_CASE_DIR="${PRISM_CASE_ROOT}/case-${PRISM_CASE_SEQ}"
    PRISM_REPO="${PRISM_CASE_DIR}/repo"
    rm -rf "${PRISM_CASE_DIR}"
    mkdir -p "${PRISM_REPO}/scripts" || exit 1

    if ! git -C "${PRISM_REPO}" init -q -b main 2>/dev/null; then
        git -C "${PRISM_REPO}" init -q
        git -C "${PRISM_REPO}" symbolic-ref HEAD refs/heads/main
    fi
    git -C "${PRISM_REPO}" config user.name "gate-test"
    git -C "${PRISM_REPO}" config user.email "gate-test@example.invalid"
    git -C "${PRISM_REPO}" config core.autocrlf false

    # 整棵 scripts/ 一起复制：钩子、安装脚本、检查入口在夹具里的相对路径与仓库一致。
    cp -r "${PRISM_TEST_ROOT}/.githooks" "${PRISM_REPO}/.githooks"
    cp "${PRISM_TEST_ROOT}/.clang-format" "${PRISM_TEST_ROOT}/.clang-tidy" "${PRISM_REPO}/"
    cp "${PRISM_TEST_ROOT}/.gitattributes" "${PRISM_REPO}/"
    cp -r "${PRISM_TEST_ROOT}/scripts/." "${PRISM_REPO}/scripts/"
}

drop_fixture() {
    [ -n "${PRISM_CASE_DIR:-}" ] && rm -rf "${PRISM_CASE_DIR}"
    PRISM_CASE_DIR=""
}

start_case() {
    printf '\n-- %s\n' "$1"
    drop_fixture
    new_fixture
}

# 用真 clang-format 生成一定合规的内容，免得夹具自己手写格式被判违规。
good_cpp() {
    printf 'int PrismSample(int a, int b) {\n    return a + b;\n}\n' \
        | "${PRISM_CLANG_FORMAT_REAL}" --assume-filename=sample.cpp -
}

# 两处违规：空格不规整 + 一行超过 ColumnLimit 100。
bad_cpp() {
    cat <<'EOF'
int  PrismSample(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l) {
      return   a + b;
}
EOF
}

stage() { git -C "${PRISM_REPO}" add "$@"; }
commit_all() {
    git -C "${PRISM_REPO}" add -A
    git -C "${PRISM_REPO}" commit -q -m "$1"
}
commit_base() {
    commit_all "$1"
    git -C "${PRISM_REPO}" rev-parse HEAD
}

finish() {
    _prism_label=${1:-门禁自测}
    drop_fixture
    rm -rf "${PRISM_CASE_ROOT}"
    printf '\n========================================\n'
    printf '通过 %s 项，失败 %s 项\n' "${PRISM_PASS}" "${PRISM_FAIL}"
    if [ "${PRISM_FAIL}" -ne 0 ]; then
        printf '%s失败。\n' "${_prism_label}" >&2
        exit 1
    fi
    printf '%s全部通过。\n' "${_prism_label}"
    exit 0
}
