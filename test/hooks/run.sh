#!/bin/sh
# 门禁自测夹具（#64 决策 25）：临时仓库 → 写夹具 → stage → 子进程跑钩子 → 断言退出码与关键字。
# 覆盖 #67（文件名/版本/安装）、#68（格式）、#69（clang-tidy）、#70（前置条件提示）。
# 不依赖 PowerShell / cmd / Python。用法：sh test/hooks/run.sh

set -u

PRISM_TEST_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "${PRISM_TEST_ROOT}" || exit 1

# shellcheck source=.githooks/common.sh
. "${PRISM_TEST_ROOT}/.githooks/common.sh"

PRISM_RUN_SH=$(command -v sh) || PRISM_RUN_SH=/bin/sh
# 覆盖环境变量一律走 env(1)：这台机器的 sh 用 `VAR=x cmd` 前缀赋值时不会把变量传给子进程
# （`export` 也一样），用 env 才能确实生效。真实钩子由 git 用自带 sh 调起，不受这条影响。
PRISM_CLANG_FORMAT_REAL=$(prism_find_tool clang-format || :)
PRISM_CLANGXX_REAL=$(prism_find_tool clang++ || :)

# 受限环境（本机沙箱）里，从 sh 起的 pwsh 拿不到原生命令的 stdout：`git --version` 会是空字符串，
# $LASTEXITCODE 也是空。install-hooks.ps1 的自校验因此必然误判，探测出来就只校验「钩子真装上了」。
PRISM_PWSH_SEES_NATIVE_OUTPUT=0
if command -v pwsh >/dev/null 2>&1; then
    if [ -n "$(pwsh -NoProfile -Command '& git.exe --version' 2>/dev/null || :)" ]; then
        PRISM_PWSH_SEES_NATIVE_OUTPUT=1
    fi
fi

PRISM_PASS=0
PRISM_FAIL=0
PRISM_CASE_NAME=""

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

# run_in <目录> <命令...>：cwd 切到目录执行，合并捕获 stdout+stderr，stdin 接 /dev/null 防卡住。
run_in() {
    _prism_dir=$1
    shift
    PRISM_OUT=$(cd "${_prism_dir}" && "$@" 2>&1 < /dev/null)
    PRISM_STATUS=$?
}

# ---------- 夹具 ----------
# 夹具放在仓库的 build/ 下（已 gitignore）。不要用 /tmp：Git Bash 的 /tmp 与 git.exe 认的
# /tmp 不是同一个目录（前者是 MSYS 安装树的 tmp，后者是 %TEMP%），-C 会直接找不到目录。
PRISM_CASE_ROOT="${PRISM_TEST_ROOT}/build/hook-tests"
PRISM_CASE_SEQ=0

# 每个用例一个全新 git 仓库，拷入真实的门禁文件。
new_fixture() {
    PRISM_CASE_SEQ=$((PRISM_CASE_SEQ + 1))
    PRISM_CASE_DIR="${PRISM_CASE_ROOT}/case-${PRISM_CASE_SEQ}"
    PRISM_REPO="${PRISM_CASE_DIR}/repo"
    rm -rf "${PRISM_CASE_DIR}"
    mkdir -p "${PRISM_REPO}" || exit 1

    if ! git -C "${PRISM_REPO}" init -q -b main 2>/dev/null; then
        git -C "${PRISM_REPO}" init -q
        git -C "${PRISM_REPO}" symbolic-ref HEAD refs/heads/main
    fi
    git -C "${PRISM_REPO}" config user.name "gate-test"
    git -C "${PRISM_REPO}" config user.email "gate-test@example.invalid"
    git -C "${PRISM_REPO}" config core.autocrlf false

    cp -r "${PRISM_TEST_ROOT}/.githooks" "${PRISM_REPO}/.githooks"
    cp "${PRISM_TEST_ROOT}/.clang-format" "${PRISM_TEST_ROOT}/.clang-tidy" "${PRISM_REPO}/"
    cp "${PRISM_TEST_ROOT}/.gitattributes" "${PRISM_REPO}/"
    mkdir -p "${PRISM_REPO}/scripts"
    cp "${PRISM_TEST_ROOT}/scripts/install-hooks.sh" "${PRISM_TEST_ROOT}/scripts/install-hooks.ps1" \
        "${PRISM_REPO}/scripts/"
}

drop_fixture() {
    [ -n "${PRISM_CASE_DIR:-}" ] && rm -rf "${PRISM_CASE_DIR}"
    PRISM_CASE_DIR=""
}
start_case() {
    PRISM_CASE_NAME=$1
    printf '\n-- %s\n' "${PRISM_CASE_NAME}"
    drop_fixture
    new_fixture
}

# 用真 clang-format 生成「一定合规」的内容，保证夹具本身不会因为手写格式被判违规。
good_cpp() {
    printf 'int PrismSample(int a, int b) {\n    return a + b;\n}\n' \
        | "${PRISM_CLANG_FORMAT_REAL}" --assume-filename=sample.cpp -
}

# 含两处违规：空格不规整 + 超过 ColumnLimit 100 的长行。
bad_cpp() {
    cat <<'EOF'
int  PrismSample(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l) {
      return   a + b;
}
EOF
}

stage() { git -C "${PRISM_REPO}" add "$@"; }

printf '门禁自测：仓库根 %s\n' "${PRISM_TEST_ROOT}"
printf 'clang-format: %s\n' "${PRISM_CLANG_FORMAT_REAL:-未找到}"

if [ -z "${PRISM_CLANG_FORMAT_REAL}" ]; then
    printf '错误：找不到 clang-format，无法自测（见 docs/markdown/项目框架.md 7.1）。\n' >&2
    exit 1
fi

# ============================================================
# #68 / #67：pre-commit 的格式检查
# ============================================================

start_case "pre-commit：staged 的格式违规被拦住，并给出可复制修复命令"
bad_cpp > "${PRISM_REPO}/sample.cpp"
stage sample.cpp
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status_nonzero "格式违规的提交被拒绝"
expect_out_has "sample.cpp" "报错点名了具体文件"
expect_out_has "-i -- sample.cpp" "报错含可复制的修复命令"
expect_out_has "clang-format" "报错说明是 clang-format 检查"

start_case "pre-commit：格式合规的 staged 文件放行"
good_cpp > "${PRISM_REPO}/sample.cpp"
stage sample.cpp
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status 0 "格式合规的提交放行"
expect_out_has "通过" "给出通过信息"

start_case "pre-commit：只查 index —— 工作区改坏了但没 stage，仍然放行"
good_cpp > "${PRISM_REPO}/sample.cpp"
stage sample.cpp
bad_cpp > "${PRISM_REPO}/sample.cpp"
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status 0 "未 staged 的工作区改动不影响提交"

start_case "pre-commit：反向验证 —— 工作区改好了但 index 里是坏内容，必须拦住"
bad_cpp > "${PRISM_REPO}/sample.cpp"
stage sample.cpp
good_cpp > "${PRISM_REPO}/sample.cpp"
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status_nonzero "index 里的坏内容仍然被拦"

start_case "pre-commit：非 C/C++ 文件不受格式检查影响"
printf 'int  x( ) {\n      return 1;\n}\n' > "${PRISM_REPO}/notes.md"
printf '   这不是 C++，随便写    \n' > "${PRISM_REPO}/other.txt"
stage notes.md other.txt
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status 0 "非 C/C++ 文件被忽略"

start_case "pre-commit：未 staged 的坏 C/C++ 文件不影响提交"
good_cpp > "${PRISM_REPO}/staged.cpp"
bad_cpp > "${PRISM_REPO}/untracked.cpp"
stage staged.cpp
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status 0 "只 stage 了合规文件，提交放行"
expect_out_lacks "untracked.cpp" "报错里不出现未 staged 的文件"

start_case "pre-commit：CmakeLists.txt 被拦，报错指出正确命名"
printf 'cmake_minimum_required(VERSION 3.20)\n' > "${PRISM_REPO}/CmakeLists.txt"
stage CmakeLists.txt
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status_nonzero "CmakeLists.txt 被拒绝"
expect_out_has "CMakeLists.txt" "报错指出正确命名"
expect_out_has "git mv" "报错给出改名命令"

start_case "pre-commit：正确命名的 CMakeLists.txt 放行"
printf 'cmake_minimum_required(VERSION 3.20)\n' > "${PRISM_REPO}/CMakeLists.txt"
stage CMakeLists.txt
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status 0 "CMakeLists.txt 放行"

start_case "pre-commit：vendor/** 下的 CmakeLists.txt 是例外（#39 决策 29）"
mkdir -p "${PRISM_REPO}/vendor/memorypool"
printf 'cmake_minimum_required(VERSION 3.12)\n' > "${PRISM_REPO}/vendor/memorypool/CmakeLists.txt"
stage vendor/memorypool/CmakeLists.txt
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status 0 "vendor/ 下的 CmakeLists.txt 放行"

start_case "pre-commit：工具版本不匹配时在动手之前就报错退出"
mkdir -p "${PRISM_REPO}/fake-clang/bin"
cat > "${PRISM_REPO}/fake-clang/bin/clang-format.exe" <<'EOF'
#!/bin/sh
echo "clang-format version 22.1.8"
EOF
chmod +x "${PRISM_REPO}/fake-clang/bin/clang-format.exe"
good_cpp > "${PRISM_REPO}/sample.cpp"
stage sample.cpp
PRISM_OUT=$(cd "${PRISM_REPO}" && env CLANG_ROOT="${PRISM_REPO}/fake-clang/bin" \
    "${PRISM_RUN_SH}" .githooks/pre-commit 2>&1 < /dev/null)
PRISM_STATUS=$?
expect_status_nonzero "版本不匹配被拦"
expect_out_has "版本不匹配" "报错说明原因"
expect_out_has "22.1.8" "报错给出实际版本"
expect_out_has "${PRISM_CLANG_VERSION_PREFIX}" "报错给出要求的前缀"

# ============================================================
# #67：安装脚本
# ============================================================

start_case "install-hooks.sh：设好 core.hooksPath 并校验生效"
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/install-hooks.sh
expect_status 0 "安装脚本成功"
expect_out_has "门禁已装上" "打印安装结果"
run_in "${PRISM_REPO}" git config --get core.hooksPath
expect_status 0 "能读回 hooksPath"
case "${PRISM_OUT}" in
    ".githooks") ok "hooksPath 指向 .githooks" ;;
    *) bad "hooksPath 应为 .githooks，实际 '${PRISM_OUT}'" ;;
esac

start_case "装好之后 git 真的会调用钩子：git commit 被 pre-commit 拦住"
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/install-hooks.sh
bad_cpp > "${PRISM_REPO}/sample.cpp"
git -C "${PRISM_REPO}" add sample.cpp
run_in "${PRISM_REPO}" git commit -m "should be blocked"
expect_status_nonzero "git commit 被钩子拦下"
expect_out_has "提交被拒绝" "拦下来自 pre-commit 门禁"

start_case "install-hooks.ps1：同样设好 hooksPath（无 pwsh 时跳过）"
if command -v pwsh >/dev/null 2>&1; then
    run_in "${PRISM_REPO}" pwsh -NoProfile -File scripts/install-hooks.ps1
    PRISM_PS_STATUS=${PRISM_STATUS}
    # 判据是「钩子真的装上了」：从 sh 侧读回，不依赖 pwsh 能不能拿到 git 的输出。
    run_in "${PRISM_REPO}" git config --get core.hooksPath
    case "${PRISM_OUT}" in
        ".githooks") ok "install-hooks.ps1 设好了 hooksPath" ;;
        *) bad "hooksPath 应为 .githooks，实际 '${PRISM_OUT}'" ;;
    esac
    if [ "${PRISM_PWSH_SEES_NATIVE_OUTPUT}" -eq 1 ]; then
        if [ "${PRISM_PS_STATUS}" -eq 0 ]; then
            ok "PowerShell 安装脚本退出码 0"
        else
            bad "PowerShell 安装脚本成功（期望退出码 0）"
        fi
    else
        # 受限环境（本机沙箱）里，从 sh 起的 pwsh 收不到原生命令的 stdout 与退出码，
        # 脚本自己的读回校验必然误判 —— 这不是脚本的问题，别拿它当失败。
        printf '    [SKIP] 本机 pwsh 拿不到原生命令的输出，脚本自校验的退出码不作判据\n'
    fi
else
    printf '    [SKIP] 本机没有 pwsh\n'
fi

# ============================================================
# #69：check-clang-tidy.sh
# ============================================================

start_case "check-clang-tidy：没有 C/C++ 改动时直接跳过，不产生额外耗时"
printf '# 只改文档\n' > "${PRISM_REPO}/README.md"
git -C "${PRISM_REPO}" add README.md
git -C "${PRISM_REPO}" commit -q -m base
PRISM_BASE_REF=$(git -C "${PRISM_REPO}" rev-parse HEAD)
printf '# 又改文档\n' > "${PRISM_REPO}/README.md"
git -C "${PRISM_REPO}" add README.md
git -C "${PRISM_REPO}" commit -q -m doc-only
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/check-clang-tidy.sh build "${PRISM_BASE_REF}"
expect_status 0 "没有 C/C++ 改动时通过"
expect_out_has "跳过 clang-tidy" "明确说明跳过"

start_case "check-clang-tidy：缺编译数据库时给出可读报错，不含糊通过"
bad_cpp > "${PRISM_REPO}/sample.cpp"
git -C "${PRISM_REPO}" add sample.cpp
git -C "${PRISM_REPO}" commit -q -m base
PRISM_BASE_REF=$(git -C "${PRISM_REPO}" rev-parse HEAD)
printf 'int  PrismOther( ) { return 2; }\n' > "${PRISM_REPO}/sample.cpp"
git -C "${PRISM_REPO}" add sample.cpp
git -C "${PRISM_REPO}" commit -q -m change
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/check-clang-tidy.sh build "${PRISM_BASE_REF}"
expect_status_nonzero "缺数据库时失败"
expect_out_has "找不到编译数据库" "报错说明缺数据库"
expect_out_has "CMAKE_EXPORT_COMPILE_COMMANDS" "报错指向修复方式"

# 端到端：手写一个最小编译数据库，让改变的头文件里的命名违规被查出来。
start_case "check-clang-tidy：改动头文件里的命名违规被查出来（真跑 clang-tidy）"
if [ -z "${PRISM_CLANGXX_REAL}" ] || ! command -v cygpath >/dev/null 2>&1; then
    printf '    [SKIP] 需要 clang++ 与 cygpath\n'
else
    mkdir -p "${PRISM_REPO}/include" "${PRISM_REPO}/src" "${PRISM_REPO}/build"
    cat > "${PRISM_REPO}/include/Naming.h" <<'EOF'
#pragma once

typedef int naming_sample;
EOF
    cat > "${PRISM_REPO}/src/use.cpp" <<'EOF'
#include "Naming.h"

int main() { return 0; }
EOF

    WIN_ROOT=$(cygpath -m "${PRISM_REPO}")
    WIN_CLANGXX=$(cygpath -m "${PRISM_CLANGXX_REAL}")
    case "$(uname -s)" in
        Linux) PRISM_ARGV="\"${WIN_CLANGXX}\", \"-std=c++17\"" ;;
        *)     PRISM_ARGV="\"${WIN_CLANGXX}\", \"--target=x86_64-w64-mingw32\", \"-std=c++17\"" ;;
    esac
    cat > "${PRISM_REPO}/build/compile_commands.json" <<EOF
[
  {
    "directory": "${WIN_ROOT}/build",
    "arguments": [
      ${PRISM_ARGV},
      "-I", "${WIN_ROOT}/include",
      "-c", "${WIN_ROOT}/src/use.cpp",
      "-o", "use.obj"
    ],
    "file": "${WIN_ROOT}/src/use.cpp",
    "output": "${WIN_ROOT}/build/use.obj"
  }
]
EOF

    git -C "${PRISM_REPO}" add -A
    git -C "${PRISM_REPO}" commit -q -m base
    PRISM_BASE_REF=$(git -C "${PRISM_REPO}" rev-parse HEAD)

    # 第二笔提交只改头文件：把 typedef 改成不合规的名字。
    cat > "${PRISM_REPO}/include/Naming.h" <<'EOF'
#pragma once

typedef int NamingSample;
typedef int naming_sample_bad;
EOF
    git -C "${PRISM_REPO}" add -A
    git -C "${PRISM_REPO}" commit -q -m change

    run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/check-clang-tidy.sh build "${PRISM_BASE_REF}"
    expect_status_nonzero "命名违规的推送被拒绝"
    expect_out_has "invalid case style for typedef" "报出了命名违规"
    expect_out_has "Naming.h" "点名了违规所在的头文件"
    expect_out_has "落在本次改动的文件上" "把诊断对应回本次改动的文件"

    # 把改动改回合规后应当通过，证明不是「无论如何都失败」。
    cat > "${PRISM_REPO}/include/Naming.h" <<'EOF'
#pragma once

typedef int NamingSample;
EOF
    git -C "${PRISM_REPO}" add -A
    git -C "${PRISM_REPO}" commit -q -m fix
    run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/check-clang-tidy.sh build "${PRISM_BASE_REF}"
    expect_status 0 "改回合规后通过"
    expect_out_has "0 处诊断" "报出 0 处诊断"
fi

# ============================================================
# #70：pre-push 的前置条件提示
# ============================================================

start_case "pre-push：前置条件不满足时给可读提示，不倒一堆 CMake 输出"
mkdir -p "${PRISM_REPO}/fake-clang/bin"
cat > "${PRISM_REPO}/fake-clang/bin/clang-tidy.exe" <<'EOF'
#!/bin/sh
echo "LLVM version 23.1.3"
EOF
chmod +x "${PRISM_REPO}/fake-clang/bin/clang-tidy.exe"
PRISM_OUT=$(cd "${PRISM_REPO}" && env CLANG_ROOT="${PRISM_REPO}/fake-clang/bin" \
    "${PRISM_RUN_SH}" .githooks/pre-push origin 2>&1 < /dev/null)
PRISM_STATUS=$?
expect_status_nonzero "前置条件不满足时拒绝推送"
expect_out_has "前置条件不满足" "明确说是前置条件问题"
expect_out_has "项目框架.md" "指向文档"
expect_out_lacks "CMake Error" "没有把 CMake 的原始报错倒给用户"

start_case "pre-push：clang-tidy 版本不匹配时在动手之前就报错退出"
mkdir -p "${PRISM_REPO}/fake-clang/bin"
cat > "${PRISM_REPO}/fake-clang/bin/clang-tidy.exe" <<'EOF'
#!/bin/sh
echo "LLVM version 22.1.8"
EOF
chmod +x "${PRISM_REPO}/fake-clang/bin/clang-tidy.exe"
PRISM_OUT=$(cd "${PRISM_REPO}" && env CLANG_ROOT="${PRISM_REPO}/fake-clang/bin" \
    "${PRISM_RUN_SH}" .githooks/pre-push origin 2>&1 < /dev/null)
PRISM_STATUS=$?
expect_status_nonzero "版本不匹配被拦"
expect_out_has "版本不匹配" "报错说明原因"

# ============================================================

drop_fixture
rm -rf "${PRISM_CASE_ROOT}"
printf '\n========================================\n'
printf '通过 %s 项，失败 %s 项\n' "${PRISM_PASS}" "${PRISM_FAIL}"
if [ "${PRISM_FAIL}" -ne 0 ]; then
    printf '门禁自测失败。\n' >&2
    exit 1
fi
printf '门禁自测全部通过。\n'
exit 0
