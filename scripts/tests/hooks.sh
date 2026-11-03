#!/bin/sh
# 门禁自测：临时仓库里跑真实钩子与安装脚本，断言退出码与关键字（#64 决策 25）。
# 覆盖 #67（文件名/版本/安装）、#68（格式）、#69（clang-tidy）、#70（前置条件提示）。
# 用法：sh scripts/tests/hooks.sh（或 sh scripts/tests/run.sh 一次跑两套）。

set -u

PRISM_TEST_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "${PRISM_TEST_ROOT}" || exit 1

# shellcheck source=scripts/checks/lib.sh
. "${PRISM_TEST_ROOT}/scripts/checks/lib.sh"
# shellcheck source=scripts/tests/lib.sh
. "${PRISM_TEST_ROOT}/scripts/tests/lib.sh"

PRISM_CASE_ROOT="${PRISM_TEST_ROOT}/build/hook-tests"

PRISM_CLANG_FORMAT_REAL=$(prism_find_tool clang-format || :)
PRISM_CLANGXX_REAL=$(prism_find_tool clang++ || :)
PRISM_CLANG_TIDY_REAL=$(prism_find_tool clang-tidy || :)

# 受限环境里从 sh 起的 pwsh 拿不到原生命令的 stdout（`git --version` 是空串），
# install-hooks.ps1 的自校验会误判；探测到这种情况就只校验「钩子真装上了」。
PRISM_PWSH_SEES_NATIVE_OUTPUT=0
if command -v pwsh >/dev/null 2>&1; then
    if [ -n "$(pwsh -NoProfile -Command '& git.exe --version' 2>/dev/null || :)" ]; then
        PRISM_PWSH_SEES_NATIVE_OUTPUT=1
    fi
fi

# 桩工具：只回答 --version，用于在不碰真实工具链的前提下测版本前缀检查。
# 文件名带 .exe 是必须的：prism_find_tool 先找 $CLANG_ROOT/<名>.exe，本机（Windows）走的正是这条。
install_fake_tool() {
    mkdir -p "${PRISM_REPO}/fake-clang/bin"
    {
        printf '#!/bin/sh\n'
        printf 'echo "%s version %s"\n' "$1" "$2"
    } > "${PRISM_REPO}/fake-clang/bin/$1"
    chmod +x "${PRISM_REPO}/fake-clang/bin/$1"
}

printf '门禁自测：仓库根 %s\n' "${PRISM_TEST_ROOT}"
printf 'clang-format: %s\n' "${PRISM_CLANG_FORMAT_REAL:-未找到}"
if [ -z "${PRISM_CLANG_TIDY_REAL}" ]; then
    printf '警告：找不到 clang-tidy，check-clang-tidy 的用例会 SKIP。\n' >&2
fi

if [ -z "${PRISM_CLANG_FORMAT_REAL}" ]; then
    printf '错误：找不到 clang-format，无法自测（见 docs/markdown/项目框架.md 7.1）。\n' >&2
    exit 1
fi

# ---- #68 / #67：pre-commit 的格式检查 ----

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
expect_out_has "通过：1 个 staged C/C++ 文件" "只统计了 staged 的那 1 个文件"
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

start_case "pre-commit：vendor/** 只豁免文件名，格式检查照做"
mkdir -p "${PRISM_REPO}/vendor/third_party"
bad_cpp > "${PRISM_REPO}/vendor/third_party/thing.cc"
stage vendor/third_party/thing.cc
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status_nonzero "vendor/ 下的 C/C++ 格式违规被拦"
expect_out_has "vendor/third_party/thing.cc" "报错点名 vendor 下的文件"

start_case "pre-commit：git mv 改名后的文件不会被漏掉（--diff-filter 必须带 R）"
# 先提交「内容坏、名字对」的文件：改名相似度 100%，index 里记成 R100。
bad_cpp > "${PRISM_REPO}/before.cpp"
git -C "${PRISM_REPO}" add before.cpp
git -C "${PRISM_REPO}" commit -q -m base
git -C "${PRISM_REPO}" mv before.cpp after.cpp
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status_nonzero "改名后的坏文件仍然被拦"
expect_out_has "after.cpp" "报错点名改名后的新路径"

start_case "pre-commit：git mv 改成 CmakeLists.txt 同样被拦（#67 AC 的改名路径）"
printf 'cmake_minimum_required(VERSION 3.20)\n' > "${PRISM_REPO}/CMakeLists.txt"
git -C "${PRISM_REPO}" add CMakeLists.txt
git -C "${PRISM_REPO}" commit -q -m base
git -C "${PRISM_REPO}" mv CMakeLists.txt CmakeLists.txt
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_status_nonzero "改名成 CmakeLists.txt 被拦"
expect_out_has "CmakeLists.txt" "报错点名改名后的错误命名"

start_case "pre-commit：工具版本不匹配时在动手之前就报错退出"
install_fake_tool clang-format.exe 22.1.8
good_cpp > "${PRISM_REPO}/sample.cpp"
stage sample.cpp
PRISM_OUT=$(cd "${PRISM_REPO}" && env CLANG_ROOT="${PRISM_REPO}/fake-clang/bin" \
    "${PRISM_RUN_SH}" .githooks/pre-commit 2>&1 < /dev/null)
PRISM_STATUS=$?
expect_status_nonzero "版本不匹配被拦"
expect_out_has "版本不匹配" "报错说明原因"
expect_out_has "22.1.8" "报错给出实际版本"
expect_out_has "${PRISM_CLANG_VERSION_PREFIX}" "报错给出要求的前缀"

# ---- #67：安装脚本 ----

start_case "install-hooks.sh：设好 core.hooksPath 并校验生效"
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/hooks/install-hooks.sh
expect_status 0 "安装脚本成功"
expect_out_has "门禁已装上" "打印安装结果"
expect_out_has "${PRISM_CLANG_VERSION_PREFIX}" "版本要求从 lib.sh 读出，不在安装脚本里硬写"
run_in "${PRISM_REPO}" git config --get core.hooksPath
expect_status 0 "能读回 hooksPath"
case "${PRISM_OUT}" in
    ".githooks") ok "hooksPath 指向 .githooks" ;;
    *) bad "hooksPath 应为 .githooks，实际 '${PRISM_OUT}'" ;;
esac

start_case "装好之后 git 真的会调用钩子：git commit 被 pre-commit 拦住"
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/hooks/install-hooks.sh
bad_cpp > "${PRISM_REPO}/sample.cpp"
git -C "${PRISM_REPO}" add sample.cpp
run_in "${PRISM_REPO}" git commit -m "should be blocked"
expect_status_nonzero "git commit 被钩子拦下"
expect_out_has "提交被拒绝" "拦下来自 pre-commit 门禁"

start_case "install-hooks.ps1：同样设好 hooksPath（无 pwsh 时跳过）"
if command -v pwsh >/dev/null 2>&1; then
    run_in "${PRISM_REPO}" pwsh -NoProfile -File scripts/hooks/install-hooks.ps1
    PRISM_PS_STATUS=${PRISM_STATUS}
    # 判据是「钩子真的装上了」：从 sh 侧读回，不依赖 pwsh 能否拿到 git 输出。
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
        # 受限环境里 pwsh 收不到原生命令输出，脚本自己的读回校验必然误判 —— 这是环境问题，别当失败。
        printf '    [SKIP] 本机 pwsh 拿不到原生命令的输出，脚本自校验的退出码不作判据\n'
    fi
else
    printf '    [SKIP] 本机没有 pwsh\n'
fi

# ---- #69：check-clang-tidy.sh ----

start_case "check-clang-tidy：没有 C/C++ 改动时直接跳过，不产生额外耗时"
if [ -z "${PRISM_CLANG_TIDY_REAL}" ]; then
    printf '    [SKIP] 本机没有 clang-tidy\n'
else
printf '# 只改文档\n' > "${PRISM_REPO}/README.md"
PRISM_BASE_REF=$(commit_base base)
printf '# 又改文档\n' > "${PRISM_REPO}/README.md"
git -C "${PRISM_REPO}" add README.md
git -C "${PRISM_REPO}" commit -q -m doc-only
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/checks/tidy.sh build "${PRISM_BASE_REF}"
expect_status 0 "没有 C/C++ 改动时通过"
expect_out_has "跳过 clang-tidy" "明确说明跳过"
fi

start_case "check-clang-tidy：缺编译数据库时给出可读报错，不含糊通过"
if [ -z "${PRISM_CLANG_TIDY_REAL}" ]; then
    printf '    [SKIP] 本机没有 clang-tidy\n'
else
bad_cpp > "${PRISM_REPO}/sample.cpp"
PRISM_BASE_REF=$(commit_base base)
printf 'int  PrismOther( ) { return 2; }\n' > "${PRISM_REPO}/sample.cpp"
git -C "${PRISM_REPO}" add sample.cpp
git -C "${PRISM_REPO}" commit -q -m change
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/checks/tidy.sh build "${PRISM_BASE_REF}"
expect_status_nonzero "缺数据库时失败"
expect_out_has "找不到编译数据库" "报错说明缺数据库"
expect_out_has "CMAKE_EXPORT_COMPILE_COMMANDS" "报错指向修复方式"
fi

# 端到端：手写最小编译数据库，验证改动头文件里的命名违规会被查出来（缺 cygpath 则整例 SKIP）。
start_case "check-clang-tidy：改动头文件里的命名违规被查出来（真跑 clang-tidy）"
if [ -z "${PRISM_CLANGXX_REAL}" ] || [ -z "${PRISM_CLANG_TIDY_REAL}" ] || ! command -v cygpath >/dev/null 2>&1; then
    printf '    [SKIP] 需要 clang++、clang-tidy 与 cygpath\n'
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

    PRISM_BASE_REF=$(commit_base base)

    cat > "${PRISM_REPO}/include/Naming.h" <<'EOF'
#pragma once

typedef int NamingSample;
typedef int naming_sample_bad;
EOF
    git -C "${PRISM_REPO}" add -A
    git -C "${PRISM_REPO}" commit -q -m change

    run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/checks/tidy.sh build "${PRISM_BASE_REF}"
    expect_status_nonzero "命名违规的推送被拒绝"
    expect_out_has "invalid case style for typedef" "报出了命名违规"
    expect_out_has "Naming.h" "点名了违规所在的头文件"
    expect_out_has "落在本次改动的文件上" "把诊断对应回本次改动的文件"

    # 改回合规后应当通过，证明不是「无论如何都失败」。
    cat > "${PRISM_REPO}/include/Naming.h" <<'EOF'
#pragma once

typedef int NamingSample;
EOF
    git -C "${PRISM_REPO}" add -A
    git -C "${PRISM_REPO}" commit -q -m fix
    run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/checks/tidy.sh build "${PRISM_BASE_REF}"
    expect_status 0 "改回合规后通过"
    expect_out_has "0 处诊断" "报出 0 处诊断"
fi

# ---- #70：pre-push 的前置条件提示 ----

start_case "pre-push：前置条件不满足时给可读提示，不倒一堆 CMake 输出"
install_fake_tool clang-tidy.exe 23.1.3
PRISM_OUT=$(cd "${PRISM_REPO}" && env CLANG_ROOT="${PRISM_REPO}/fake-clang/bin" \
    "${PRISM_RUN_SH}" .githooks/pre-push origin 2>&1 < /dev/null)
PRISM_STATUS=$?
expect_status_nonzero "前置条件不满足时拒绝推送"
expect_out_has "前置条件不满足" "明确说是前置条件问题"
expect_out_has "项目框架.md" "指向文档"
expect_out_lacks "CMake Error" "没有把 CMake 的原始报错倒给用户"

start_case "pre-push：clang-tidy 版本不匹配时在动手之前就报错退出"
install_fake_tool clang-tidy.exe 22.1.8
PRISM_OUT=$(cd "${PRISM_REPO}" && env CLANG_ROOT="${PRISM_REPO}/fake-clang/bin" \
    "${PRISM_RUN_SH}" .githooks/pre-push origin 2>&1 < /dev/null)
PRISM_STATUS=$?
expect_status_nonzero "版本不匹配被拦"
expect_out_has "版本不匹配" "报错说明原因"


finish "门禁自测"
