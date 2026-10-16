#!/bin/sh
# 检查入口自测（#97 / #100）：在临时 git 仓库里造变更集，断言 scripts/checks/ 下入口的退出码与 stderr。
# format.sh（#97）覆盖三种输入（全合规 → 0 / 含未格式化 → 非 0 / 空范围 → 0）、vendor 例外、<rev> 与
# <a>...<b> 两种范围写法、非 C/C++ 忽略、git 失败绝不 fail-open，以及 ci（GitHub Actions 环境解析范围）。
# tidy.sh（#100）覆盖三种输入（有违规 → 非 0 / 无 C/C++ 改动 → 0 且不要求编译数据库 / 空范围 → 0）。
# 不联网、不依赖 CI。用法：sh test/checks/run.sh

set -u

PRISM_TEST_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "${PRISM_TEST_ROOT}" || exit 1

# shellcheck source=scripts/checks/lib.sh
. "${PRISM_TEST_ROOT}/scripts/checks/lib.sh"

PRISM_RUN_SH=$(command -v sh) || PRISM_RUN_SH=/bin/sh
PRISM_CLANG_FORMAT_REAL=$(prism_find_tool clang-format || :)
# tidy.sh 的用例要真 clang-tidy（入口第一步就校验它）与真 clang++（手写编译数据库要用它当编译器）。
PRISM_CLANG_TIDY_REAL=$(prism_find_tool clang-tidy || :)
PRISM_CLANGXX_REAL=$(prism_find_tool clang++ || :)

PRISM_PASS=0
PRISM_FAIL=0

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

# 逐字比对：用来断言「pre-commit 与入口是同一份判定」。
expect_out_equals() {
    if [ "${PRISM_OUT}" = "$1" ]; then
        ok "$2"
    else
        bad "$2（输出与预期不一致）"
        printf '          预期：\n'
        printf '%s\n' "$1" | sed 's/^/            | /'
    fi
}

# run_in <目录> <命令...>：cwd 切到目录执行，合并捕获 stdout+stderr，stdin 接 /dev/null 防卡住。
run_in() {
    _prism_dir=$1
    shift
    PRISM_OUT=$(cd "${_prism_dir}" && "$@" 2>&1 < /dev/null)
    PRISM_STATUS=$?
}

# 跑检查入口；cwd 在仓库内即可，入口自己会 cd 到仓库根。
run_entry() {
    run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/checks/format.sh "$@"
}

# 跑入口的 ci token，显式传全部 CI 环境变量（含空值）：本机/CI 已有的同名变量不能漏进来影响判定。
run_entry_ci() {
    _prism_ci_event_name=$1
    _prism_ci_event_path=$2
    _prism_ci_sha=$3
    _prism_ci_base=$4
    PRISM_OUT=$(cd "${PRISM_REPO}" && env GITHUB_EVENT_NAME="${_prism_ci_event_name}" \
        GITHUB_EVENT_PATH="${_prism_ci_event_path}" GITHUB_SHA="${_prism_ci_sha}" \
        GITHUB_BASE_SHA="${_prism_ci_base}" \
        "${PRISM_RUN_SH}" scripts/checks/format.sh ci 2>&1 < /dev/null)
    PRISM_STATUS=$?
}

# 跑静态检查入口（#100）；参数是 <编译数据库目录> <变更范围>。
run_entry_tidy() {
    run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/checks/tidy.sh "$@"
}

# ---------- 夹具 ----------
# 夹具放在仓库的 build/ 下（已 gitignore）。不要用 /tmp：Git Bash 的 /tmp 与 git.exe 认的
# /tmp 不是同一个目录，-C 会直接找不到目录。
PRISM_CASE_ROOT="${PRISM_TEST_ROOT}/build/check-tests"
PRISM_CASE_SEQ=0

# 每个用例一个全新 git 仓库，拷入入口真正会用到的门禁文件。
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

    cp -r "${PRISM_TEST_ROOT}/scripts/checks" "${PRISM_REPO}/scripts/"
    cp -r "${PRISM_TEST_ROOT}/.githooks" "${PRISM_REPO}/.githooks"
    cp "${PRISM_TEST_ROOT}/.clang-format" "${PRISM_TEST_ROOT}/.clang-tidy" "${PRISM_REPO}/"
    cp "${PRISM_TEST_ROOT}/.gitattributes" "${PRISM_REPO}/"
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
commit_all() {
    git -C "${PRISM_REPO}" add -A
    git -C "${PRISM_REPO}" commit -q -m "$1"
}
commit_base() {
    commit_all "$1"
    git -C "${PRISM_REPO}" rev-parse HEAD
}

# ---------- #100：tidy.sh 用的夹具 ----------

# clang-tidy 是原生 exe，编译数据库里的 directory / file 必须是它认得的原生路径：Windows 上得是
# D:/...，MSYS 的 /d/... 一条也匹配不上 —— clang-tidy 自己把编译单元绝对化成 D:/...，于是查不到
# 编译命令，只打印「Compile command not found」并以 0 退出（假绿）。MSYS2 / Git Bash 的 `pwd -W`
# 直接给出原生形式；Linux 没有 -W，退回 pwd -P（那里本来就是原生形式）。
# 刻意不用 cygpath：它只在 MSYS 系存在，会多出一条「本机能过、CI 不能过」的路径。
fixture_native_dir() {
    if _prism_native=$(cd "${PRISM_REPO}" && pwd -W 2>/dev/null) && [ -n "${_prism_native}" ]; then
        _prism_native_out=${_prism_native}
    else
        _prism_native_out=$(cd "${PRISM_REPO}" && pwd -P)
    fi
    # 编译数据库是 JSON：路径里出现反斜杠（`\P` 之类）会直接把文件读坏，一律换成正斜杠。
    printf '%s\n' "${_prism_native_out}" | tr '\\' '/'
}

# 夹具自己的最小规则集，不复用仓库根的 .clang-tidy：那份规则集由别的工单在改，自测跟着它漂就不是
# 在测入口本身。InheritParentConfig: false 是关键 —— 夹具就在仓库的 build/ 下，不拦住向上查找，
# 仓库根那份配置会被并进来。
write_tidy_config() {
    cat > "${PRISM_REPO}/.clang-tidy" <<'EOF'
InheritParentConfig: false
Checks: 'readability-identifier-naming'
WarningsAsErrors: 'readability-identifier-naming'
CheckOptions:
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
EOF
}

# 夹具里没有 CMake，编译数据库手写。command 里用真 clang++（prism_find_tool 解析出的路径，反斜杠
# 换成 JSON 安全的正斜杠）。clang-tidy 只解析这条命令的参数、不执行它，所以 argv[0] 用 POSIX 形式
# 也无妨；所有路径都从夹具自己的 pwd 推出来。
write_tidy_db() {
    mkdir -p "${PRISM_REPO}/build"
    _prism_tidy_native=$(fixture_native_dir)
    _prism_tidy_cxx=$(printf '%s\n' "${PRISM_CLANGXX_REAL}" | tr '\\' '/')
    cat > "${PRISM_REPO}/build/compile_commands.json" <<EOF
[
  {
    "directory": "${_prism_tidy_native}/build",
    "file": "${_prism_tidy_native}/src/sample.cpp",
    "command": "${_prism_tidy_cxx} -std=c++17 -c ${_prism_tidy_native}/src/sample.cpp -o ${_prism_tidy_native}/build/sample.o"
  }
]
EOF
}

printf '检查入口自测：仓库根 %s\n' "${PRISM_TEST_ROOT}"
printf '入口: %s\n' "${PRISM_TEST_ROOT}/scripts/checks/format.sh"
printf '入口: %s\n' "${PRISM_TEST_ROOT}/scripts/checks/tidy.sh"
printf 'clang-format: %s\n' "${PRISM_CLANG_FORMAT_REAL:-未找到}"
printf 'clang-tidy:   %s\n' "${PRISM_CLANG_TIDY_REAL:-未找到}"
printf 'clang++:      %s\n' "${PRISM_CLANGXX_REAL:-未找到}"
if [ -z "${PRISM_CLANG_FORMAT_REAL}" ]; then
    printf '错误：找不到 clang-format，无法自测（见 docs/markdown/项目框架.md 7.1）。\n' >&2
    exit 1
fi

# ============================================================
# AC1/AC3：三种输入 —— 全合规 / 未格式化 / 空范围
# ============================================================

start_case "入口：全合规的变更集 → 0，通过文案与 pre-commit 逐字一致"
good_cpp > "${PRISM_REPO}/sample.cpp"
stage sample.cpp
run_entry staged
expect_status 0 "全合规放行"
expect_out_has "通过：1 个 staged C/C++ 文件" "通过文案与 pre-commit 完全一致"

start_case "入口：含未格式化文件 → 非 0，点名文件并给出可复制的修复命令"
bad_cpp > "${PRISM_REPO}/sample.cpp"
stage sample.cpp
run_entry staged
expect_status_nonzero "格式违规被拦"
expect_out_has "sample.cpp" "报错点名了具体文件"
expect_out_has "-i -- sample.cpp" "报错含可复制的修复命令"
expect_out_has "一次修完" "给出批量修复命令"
expect_out_has "clang-format" "报错说明是 clang-format 检查"

start_case "入口：空变更范围 → 0（git 成功但列表为空，不是错误）"
printf '# base\n' > "${PRISM_REPO}/README.md"
commit_all base
run_entry HEAD
expect_status 0 "完全空的范围放行"
expect_out_has "为空" "明确说明范围为空"

start_case "入口：只有文档改动 → 0，且不误报 C/C++"
printf '# base\n' > "${PRISM_REPO}/README.md"
commit_all base
printf '# base\n\n文档更新。\n' > "${PRISM_REPO}/README.md"
commit_all doc-only
run_entry HEAD~1
expect_status 0 "只有文档改动放行"
expect_out_has "通过" "给出通过信息"

# ============================================================
# AC2：pre-commit 调用同一份入口
# ============================================================

start_case "入口：pre-commit 与 format.sh staged 是同一份判定（输出逐字相同）"
bad_cpp > "${PRISM_REPO}/sample.cpp"
stage sample.cpp
run_entry staged
PRISM_ENTRY_OUT=${PRISM_OUT}
PRISM_ENTRY_STATUS=${PRISM_STATUS}
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_out_equals "${PRISM_ENTRY_OUT}" "pre-commit 的输出与入口 staged 逐字相同"
if [ "${PRISM_STATUS}" -eq "${PRISM_ENTRY_STATUS}" ]; then
    ok "退出码也一致"
else
    bad "退出码一致（pre-commit ${PRISM_STATUS} vs 入口 ${PRISM_ENTRY_STATUS}）"
fi

start_case "入口：合规路径下 pre-commit 与入口输出同样逐字相同"
good_cpp > "${PRISM_REPO}/sample.cpp"
stage sample.cpp
run_entry staged
PRISM_ENTRY_OUT=${PRISM_OUT}
run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" .githooks/pre-commit
expect_out_equals "${PRISM_ENTRY_OUT}" "合规路径下两边输出逐字相同"
expect_status 0 "两边都放行"

# ============================================================
# AC1：vendor/** 只豁免文件名检查
# ============================================================

start_case "入口：vendor/** 下的 CmakeLists.txt 跳过文件名检查，格式检查照做"
mkdir -p "${PRISM_REPO}/vendor/memorypool"
printf 'cmake_minimum_required(VERSION 3.12)\n' > "${PRISM_REPO}/vendor/memorypool/CmakeLists.txt"
stage vendor/memorypool/CmakeLists.txt
run_entry staged
expect_status 0 "vendor 下的 CmakeLists.txt 跳过文件名检查"
expect_out_lacks "git mv" "不给出改名命令"

mkdir -p "${PRISM_REPO}/vendor/third_party"
good_cpp > "${PRISM_REPO}/vendor/third_party/ok.cc"
stage vendor/third_party/ok.cc
run_entry staged
expect_status 0 "vendor 下合规的 C/C++ 放行（格式检查照做）"

bad_cpp > "${PRISM_REPO}/vendor/third_party/thing.cc"
stage vendor/third_party/thing.cc
run_entry staged
expect_status_nonzero "vendor 下的 C/C++ 格式违规仍被拦"
expect_out_has "vendor/third_party/thing.cc" "报错点名 vendor 下的文件"

# ============================================================
# AC1/AC3：范围写法 —— <rev> 与 <a>...<b>
# ============================================================

start_case "入口：<rev> 范围里 vendor 之外的 CmakeLists.txt → 非 0 并给出 git mv"
printf '# base\n' > "${PRISM_REPO}/README.md"
PRISM_REV_BASE=$(commit_base base)
printf 'cmake_minimum_required(VERSION 3.20)\n' > "${PRISM_REPO}/CmakeLists.txt"
commit_all "bad name"
run_entry "${PRISM_REV_BASE}"
expect_status_nonzero "revision 范围里 CmakeLists.txt 被拦"
expect_out_has "文件名错误" "报出文件名问题"
expect_out_has "git mv" "给出改名命令"
expect_out_has "改动的文件" "revision 模式用「改动」措辞，不用 staged"
expect_out_lacks "staged" "revision 模式不出现 staged 字样"

start_case "入口：<rev> 取 HEAD 内容，<a>...<b> 取 <b> 内容（真跑提交，验证 ... 语义）"
printf '# base\n' > "${PRISM_REPO}/README.md"
PRISM_REV_BASE=$(commit_base base)

git -C "${PRISM_REPO}" checkout -q -b side
bad_cpp > "${PRISM_REPO}/sample.cpp"
commit_all "side: bad"
PRISM_REV_SIDE=$(git -C "${PRISM_REPO}" rev-parse HEAD)

git -C "${PRISM_REPO}" checkout -q main
good_cpp > "${PRISM_REPO}/sample.cpp"
commit_all "main: good"
PRISM_REV_MAIN=$(git -C "${PRISM_REPO}" rev-parse HEAD)

# <rev>：文件取 <rev>...HEAD，内容取 HEAD。
run_entry "${PRISM_REV_BASE}"
expect_status 0 "<rev> 模式内容取 HEAD（HEAD 上是合规内容）"
expect_out_has "通过：1 个改动的 C/C++ 文件" "revision 模式统计到了改动的那个文件"

git -C "${PRISM_REPO}" checkout -q side
run_entry "${PRISM_REV_BASE}"
expect_status_nonzero "<rev> 模式内容取 HEAD（HEAD 是坏内容，必须拦）"
expect_out_has "sample.cpp" "点名坏文件"
git -C "${PRISM_REPO}" checkout -q main

# <a>...<b>：两条分叉的支线，内容取 <b> —— 同一对提交，方向反过来结论就相反。
run_entry "${PRISM_REV_SIDE}...${PRISM_REV_MAIN}"
expect_status 0 "a...b 取 b（main 上的合规内容）"
run_entry "${PRISM_REV_MAIN}...${PRISM_REV_SIDE}"
expect_status_nonzero "a...b 取 b（side 上的坏内容）"
expect_out_has "sample.cpp" "点名 side 上的坏文件"

# ============================================================
# AC3：非 C/C++ 忽略、改名不漏、非 ASCII 文件名
# ============================================================

start_case "入口：非 C/C++ 文件被忽略"
printf '  这不是 C++，随便写   \n' > "${PRISM_REPO}/notes.md"
printf 'int  x( ) {\n      return 1;\n}\n' > "${PRISM_REPO}/other.txt"
stage notes.md other.txt
run_entry staged
expect_status 0 "非 C/C++ 文件被忽略"
expect_out_lacks "notes.md" "报错/统计里不出现它们"

start_case "入口：git mv 改名后的文件不会被漏掉（--diff-filter 必须带 R）"
bad_cpp > "${PRISM_REPO}/before.cpp"
commit_base base
git -C "${PRISM_REPO}" mv before.cpp after.cpp
run_entry staged
expect_status_nonzero "改名后的坏文件仍然被拦"
expect_out_has "after.cpp" "报错点名改名后的新路径"

start_case "入口：非 ASCII 文件名也能正确定位（-z + tr 抗 core.quotePath）"
bad_cpp > "${PRISM_REPO}/样例.cpp"
stage 样例.cpp
run_entry staged
expect_status_nonzero "非 ASCII 文件名的违规被拦"
expect_out_has "样例.cpp" "报错里的文件名没有被转义"
expect_out_has "-i -- 样例.cpp" "修复命令可直接复制"

# ============================================================
# AC3/AC8：git 失败绝不 fail-open
# ============================================================

start_case "入口：取不到变更范围 / 参数不合法 → 非 0，绝不静默放行"
run_entry no-such-rev-abc123
expect_status_nonzero "不存在的 revision 被拦"
expect_out_has "门禁" "报错带门禁前缀"
expect_out_has "读取变更范围失败" "明确说是范围取不到"

run_entry "no-such-rev-abc123...HEAD"
expect_status_nonzero "a...b 里 a 不存在也被拦"

run_entry "HEAD...no-such-rev-abc123"
expect_status_nonzero "a...b 里 b 不存在也被拦"

run_entry "no-such-rev-abc123...no-such-rev-def456"
expect_status_nonzero "两端都不存在也被拦"

run_in "${PRISM_REPO}" "${PRISM_RUN_SH}" scripts/checks/format.sh
expect_status_nonzero "缺范围参数被拦"
expect_out_has "用法" "给出用法说明"

run_entry --help
expect_status 0 "--help 放行"
expect_out_has "staged" "usage 里写明三种范围写法"

run_entry --nope
expect_status_nonzero "不认识的参数被拦"

# ============================================================
# AC6（CI 同源）：ci token 从 GitHub Actions 环境解析范围
# ============================================================

start_case "入口：ci 从 GitHub Actions 环境解析变更范围（pull_request / push）"
printf '# base\n' > "${PRISM_REPO}/README.md"
PRISM_REV_BASE=$(commit_base base)
printf '# base\n\nv2\n' > "${PRISM_REPO}/README.md"
bad_cpp > "${PRISM_REPO}/sample.cpp"
commit_all "bad cpp"
PRISM_REV_BAD=$(git -C "${PRISM_REPO}" rev-parse HEAD)
good_cpp > "${PRISM_REPO}/sample.cpp"
commit_all "good cpp"
PRISM_REV_GOOD=$(git -C "${PRISM_REPO}" rev-parse HEAD)

# 事件 JSON 照抄 GitHub 现在的真实形状：键按**字母序**排，所以 base / head 里的 `repo`（一个很大的
# 嵌套对象）排在 `sha` **之前**。user / repo 里各放一个同名 sha 诱饵，且都排在真 sha 前面：
# 「父键之后遇到的第一个 sha」这种朴素取法会直接拿到诱饵，`git diff 诱饵...head` 报
# unknown revision，范围判定整条失效。（#98 的第一次 CI 运行就是这么红的：base 未取到。）
cat > "${PRISM_REPO}/event-pr.json" <<EOF
{
  "action": "opened",
  "after": "${PRISM_REV_GOOD}",
  "before": "${PRISM_REV_BASE}",
  "number": 97,
  "pull_request": {
    "additions": 12,
    "base": {
      "label": "example:main",
      "ref": "main",
      "repo": {
        "id": 1,
        "default_branch": "main",
        "sha": "decoy-in-base-repo"
      },
      "sha": "${PRISM_REV_BASE}",
      "user": {
        "login": "example",
        "sha": "decoy-in-base-user"
      }
    },
    "head": {
      "label": "example:topic",
      "ref": "topic",
      "repo": {
        "id": 1,
        "sha": "decoy-in-head-repo"
      },
      "sha": "${PRISM_REV_GOOD}",
      "user": {
        "login": "example",
        "sha": "decoy-in-head-user"
      }
    },
    "url": "https://api.github.com/repos/example/repo/pulls/97"
  }
}
EOF

# head 取自事件 JSON（REV_GOOD，合规），不是 GITHUB_SHA（REV_BAD，坏）：所以放行。
run_entry_ci pull_request "${PRISM_REPO}/event-pr.json" "${PRISM_REV_BAD}" ""
expect_status 0 "pull_request：head 取自事件 JSON 而不是 GITHUB_SHA"
expect_out_has "通过：1 个改动的 C/C++ 文件" "范围落在 base...head 上"

# base 以 GITHUB_BASE_SHA 优先（= head，范围为空）；若退回 JSON 的 base 就不是空范围了。
run_entry_ci pull_request "${PRISM_REPO}/event-pr.json" "${PRISM_REV_BAD}" "${PRISM_REV_GOOD}"
expect_status 0 "pull_request：base 以 GITHUB_BASE_SHA 优先"
expect_out_has "为空" "范围确实按 GITHUB_BASE_SHA 收敛成空"

# push：base 取事件里的 .before，head 取 GITHUB_SHA。
cat > "${PRISM_REPO}/event-push.json" <<EOF
{
  "ref": "refs/heads/main",
  "before": "${PRISM_REV_BASE}",
  "after": "${PRISM_REV_BAD}",
  "created": false,
  "deleted": false,
  "forced": false,
  "base_ref": null,
  "commits": [
    {
      "id": "${PRISM_REV_BAD}",
      "message": "bad cpp",
      "before": "decoy-in-commit",
      "added": [
        "sample.cpp"
      ]
    }
  ]
}
EOF
run_entry_ci push "${PRISM_REPO}/event-push.json" "${PRISM_REV_BAD}" ""
expect_status_nonzero "push：.before..GITHUB_SHA 的坏内容被拦"
expect_out_has "sample.cpp" "点名坏文件"
expect_out_lacks "decoy-in-commit" "顶层 .before 没被 commits[] 里的同名键顶掉"

# .before 是全零（新建分支）时不能悄悄把整个历史当成变更范围。
cat > "${PRISM_REPO}/event-push-zero.json" <<EOF
{
  "ref": "refs/heads/main",
  "before": "0000000000000000000000000000000000000000",
  "after": "${PRISM_REV_BAD}"
}
EOF
run_entry_ci push "${PRISM_REPO}/event-push-zero.json" "${PRISM_REV_BAD}" ""
expect_status_nonzero "push：.before 全零被拦"
expect_out_has "before" "报错点出 .before"

start_case "入口：ci 在没有可用的 CI 环境时硬失败（空范围是 fail-open）"
printf '# base\n' > "${PRISM_REPO}/README.md"
PRISM_REV_BASE=$(commit_base base)

run_entry_ci "" "${PRISM_REPO}/event-pr.json" "${PRISM_REV_BASE}" ""
expect_status_nonzero "缺 GITHUB_EVENT_NAME 被拦"
expect_out_has "GITHUB_EVENT_NAME" "报错点出缺哪个变量"

run_entry_ci workflow_dispatch "${PRISM_REPO}/event-pr.json" "${PRISM_REV_BASE}" ""
expect_status_nonzero "不认识的事件类型被拦"
expect_out_has "GITHUB_EVENT_NAME" "报错说明支持哪些事件"

run_entry_ci pull_request "${PRISM_REPO}/no-such-event.json" "${PRISM_REV_BASE}" ""
expect_status_nonzero "事件文件不存在且没有 GITHUB_BASE_SHA 时被拦"

# ============================================================
# #100：共享静态检查入口 tidy.sh —— 三种输入
# 有违规 → 非 0（点名规则与文件）/ 无 C/C++ 改动 → 0（且不要求编译数据库）/ 变更范围为空 → 0
# ============================================================

start_case "入口（tidy.sh）：有违规 → 非 0，报出规则名与违规文件"
if [ -z "${PRISM_CLANG_TIDY_REAL}" ] || [ -z "${PRISM_CLANGXX_REAL}" ]; then
    printf '    [SKIP] 本机缺少 clang-tidy 或 clang++，违规用例跳过（需要真工具跑真诊断；不计为通过）\n'
else
printf '# base\n' > "${PRISM_REPO}/README.md"
write_tidy_config
PRISM_REV_BASE=$(commit_base base)

mkdir -p "${PRISM_REPO}/src"
# 只违反 ClassCase: CamelCase 一条（变量命名不在夹具的规则集里）。
cat > "${PRISM_REPO}/src/sample.cpp" <<'EOF'
class bad_name {
public:
    int value = 0;
};

int main() { return 0; }
EOF
commit_all "naming violation"
write_tidy_db

run_entry_tidy build "${PRISM_REV_BASE}"
expect_status_nonzero "命名违规被拦"
expect_out_has "readability-identifier-naming" "报出违规的规则名"
expect_out_has "sample.cpp" "点名违规文件"
expect_out_has "invalid case style for class 'bad_name'" "报出具体诊断"
expect_out_has "落在本次改动的文件上" "把诊断对应回本次改动的文件"
fi

start_case "入口（tidy.sh）：无 C/C++ 改动 → 0，明确跳过且不要求编译数据库"
if [ -z "${PRISM_CLANG_TIDY_REAL}" ]; then
    printf '    [SKIP] 本机没有 clang-tidy，用例跳过（不计为通过）\n'
else
printf '# base\n' > "${PRISM_REPO}/README.md"
write_tidy_config
PRISM_REV_BASE=$(commit_base base)
printf '# base\n\n文档更新。\n' > "${PRISM_REPO}/README.md"
commit_all doc-only

# 夹具故意不建 build/：这一步就是要证明「没有 C/C++ 改动」根本不碰编译数据库。
run_entry_tidy build "${PRISM_REV_BASE}"
expect_status 0 "只有文档改动时放行"
expect_out_has "没有 C/C++ 改动" "明确说明范围内没有 C/C++ 改动"
expect_out_has "跳过 clang-tidy" "明确说跳过 clang-tidy"
expect_out_lacks "找不到编译数据库" "没有因为缺编译数据库而拦人"
if [ -d "${PRISM_REPO}/build" ]; then
    bad "入口造出了构建目录：这一步必须完全不碰编译数据库"
else
    ok "跑完后夹具里仍然没有构建目录（确实没碰编译数据库）"
fi
fi

start_case "入口（tidy.sh）：变更范围为空 → 0（git 成功但列表为空，不是错误）"
if [ -z "${PRISM_CLANG_TIDY_REAL}" ]; then
    printf '    [SKIP] 本机没有 clang-tidy，用例跳过（不计为通过）\n'
else
printf '# base\n' > "${PRISM_REPO}/README.md"
write_tidy_config
PRISM_REV_BASE=$(commit_base base)

# <rev> 写法：git diff HEAD...HEAD 拿不出任何改动。
run_entry_tidy build HEAD
expect_status 0 "空范围放行"
expect_out_has "没有 C/C++ 改动" "空范围同样走「没有 C/C++ 改动」这条放行路"

# <a>...<b> 写法：两端指向同一个提交，同样是空范围。
run_entry_tidy build "${PRISM_REV_BASE}...${PRISM_REV_BASE}"
expect_status 0 "两端相同的 a...b 空范围也放行"
fi

# ============================================================

drop_fixture
rm -rf "${PRISM_CASE_ROOT}"
printf '\n========================================\n'
printf '通过 %s 项，失败 %s 项\n' "${PRISM_PASS}" "${PRISM_FAIL}"
if [ "${PRISM_FAIL}" -ne 0 ]; then
    printf '检查入口自测失败。\n' >&2
    exit 1
fi
printf '检查入口自测全部通过。\n'
exit 0
