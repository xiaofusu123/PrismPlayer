#!/usr/bin/env bash
# Linux CI 前置（spec #96 决策 12/13/15）：把干净的 ubuntu-24.04 runner 配成与成员本机同源的工具链。
# 契约：版本判定复用门禁自己的 prism_require_tool（scripts/checks/lib.sh），CI 与本机只有一份实现。
# 用法：[组件...] 取 llvm cmake system vcpkg（默认 llvm）；--clang-prefix / --vcpkg-baseline 只打印。
# 退出码：0 成功（已装则跳过）；非 0 表示某一步失败，绝不静默降级。

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
LLVM_MAJOR=23
LLVM_BIN="/usr/lib/llvm-${LLVM_MAJOR}/bin"
LLVM_KEYRING=/usr/share/keyrings/apt.llvm.org.gpg
KITWARE_KEYRING=/usr/share/keyrings/kitware-archive-keyring.gpg

say() { printf '%s\n' "$*"; }
die() {
    printf '错误：%s\n' "$*" >&2
    exit 1
}

# 版本前缀的单一事实来源：scripts/checks/lib.sh（本机钩子读的是同一个常量）。
clang_prefix() {
    sed -n 's/^[[:space:]]*PRISM_CLANG_VERSION_PREFIX="\([^"]*\)".*$/\1/p' \
        "${PROJECT_ROOT}/scripts/checks/lib.sh" | head -n 1
}

# vcpkg 的 baseline 也只有一个来源：vcpkg-configuration.json 的 default-registry。
vcpkg_baseline() {
    sed -n 's/.*"baseline"[[:space:]]*:[[:space:]]*"\([0-9a-fA-F]\{40\}\)".*/\1/p' \
        "${PROJECT_ROOT}/vcpkg-configuration.json" | head -n 1
}

install_llvm() {
    if [ -x "${LLVM_BIN}/clang++" ] && [ -x "${LLVM_BIN}/clang-format" ] && [ -x "${LLVM_BIN}/clang-tidy" ]; then
        say "llvm：${LLVM_BIN} 已就绪，跳过安装"
        return 0
    fi

    say "llvm：从 apt.llvm.org 的 noble-${LLVM_MAJOR} 仓安装 clang / clang-format / clang-tidy ..."
    sudo apt-get install -y --no-install-recommends ca-certificates curl gnupg
    curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key \
        | sudo gpg --dearmor --yes -o "${LLVM_KEYRING}"
    printf 'deb [signed-by=%s] https://apt.llvm.org/noble/ llvm-toolchain-noble-%s main\n' \
        "${LLVM_KEYRING}" "${LLVM_MAJOR}" \
        | sudo tee "/etc/apt/sources.list.d/llvm-${LLVM_MAJOR}.list" > /dev/null
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends \
        "clang-${LLVM_MAJOR}" \
        "clang-format-${LLVM_MAJOR}" \
        "clang-tidy-${LLVM_MAJOR}" \
        "clang-tools-${LLVM_MAJOR}" \
        "lld-${LLVM_MAJOR}" \
        "libclang-rt-${LLVM_MAJOR}-dev"
}

# 预设（CMakePresets.json 的 linux-base）把编译器写成裸 `clang` / `clang++`，所以 PATH 上第一个 clang 就是构建用的。
# runner 自带的 /usr/bin/clang 是 18：只校验 ${LLVM_BIN} 会漏掉它，#99 的首次真实运行就是这么红的。
prepend_llvm_path() {
    local tool versioned
    for tool in clang clang++ clang-format clang-tidy; do
        if [ ! -x "${LLVM_BIN}/${tool}" ]; then
            versioned="${LLVM_BIN}/${tool}-${LLVM_MAJOR}"
            [ -x "${versioned}" ] || die "${LLVM_BIN} 里既没有 ${tool} 也没有 ${tool}-${LLVM_MAJOR}。"
            sudo ln -sf "${versioned}" "${LLVM_BIN}/${tool}"
        fi
    done

    if [ -n "${GITHUB_PATH:-}" ]; then
        printf '%s\n' "${LLVM_BIN}" >> "${GITHUB_PATH}"
        say "llvm：${LLVM_BIN} 会加到后续步骤 PATH 的最前面（预设用裸 clang / clang++）。"
    else
        say "llvm：本机运行，不写 GITHUB_PATH（只有 CI 需要）。"
    fi
}

install_cmake() {
    local version
    version=$(cmake --version 2>/dev/null | sed -n 's/^cmake version \([0-9]*\).*$/\1/p' || true)
    if [ -n "${version}" ] && [ "${version}" -ge 4 ]; then
        say "cmake：已是 $(cmake --version | head -n 1)，跳过安装"
        return 0
    fi

    say "cmake：从 Kitware 的 noble 仓安装 4.x（Ubuntu 24.04 自带的是 3.28）..."
    sudo apt-get install -y --no-install-recommends ca-certificates curl gnupg
    curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc \
        | sudo gpg --dearmor --yes -o "${KITWARE_KEYRING}"
    printf 'deb [signed-by=%s] https://apt.kitware.com/ubuntu/ noble main\n' "${KITWARE_KEYRING}" \
        | sudo tee /etc/apt/sources.list.d/kitware.list > /dev/null
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends cmake
}

# glfw3 / vulkan / ffmpeg 在 Linux 侧的 vcpkg port 需要这些系统开发包（#96 决策 15）。
# 干净 runner 缺 autoreconf 一族会 BUILD_FAILED：`apt-get install -s` 只证明包名可解析，证明不了清单够用。
install_system_deps() {
    say "system：安装 glfw3 / vulkan / ffmpeg 在 Linux 侧的系统开发包 ..."
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends \
        pkg-config \
        ninja-build \
        binutils \
        nasm yasm \
        autoconf autoconf-archive automake libtool libltdl-dev \
        bison flex \
        libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
        libxi-dev libxfixes-dev libxss-dev libxxf86vm-dev libxcb1-dev libx11-xcb-dev \
        libgl-dev libglu1-mesa-dev libegl-dev mesa-common-dev \
        libvulkan-dev \
        libwayland-dev wayland-protocols libxkbcommon-dev \
        libdrm-dev \
        libasound2-dev \
        python3
}

# CI 上 vcpkg 不在 $HOME，要自己取一份，且必须落在仓库钉死的 baseline 上（#96 决策 14）。
install_vcpkg() {
    local root="${VCPKG_ROOT:-${HOME}/vcpkg}"
    local baseline
    baseline=$(vcpkg_baseline)
    [ -n "${baseline}" ] || die "读不出版 baseline（vcpkg-configuration.json 的 default-registry.baseline）。"

    export VCPKG_ROOT="${root}"

    if [ -d "${root}/.git" ] && [ -x "${root}/vcpkg" ]; then
        say "vcpkg：${root} 已就绪（$(git -C "${root}" rev-parse --short HEAD 2>/dev/null || echo '?')），跳过克隆"
    else
        say "vcpkg：取 ${root} 并切到 baseline ${baseline} ..."
        mkdir -p "${root}"
        git -C "${root}" init -q
        if git -C "${root}" remote get-url origin > /dev/null 2>&1; then
            git -C "${root}" remote set-url origin https://github.com/microsoft/vcpkg
        else
            git -C "${root}" remote add origin https://github.com/microsoft/vcpkg
        fi
        # 按 baseline 取一个浅克隆：端口树必须正好是仓库钉死的那一次。
        if ! git -C "${root}" fetch --depth 1 origin "${baseline}"; then
            die "取不到 vcpkg 的 baseline ${baseline}（网络或该提交不可达）。"
        fi
        git -C "${root}" checkout -q --detach FETCH_HEAD
        "${root}/bootstrap-vcpkg.sh" -disableMetrics
    fi

    local head
    head=$(git -C "${root}" rev-parse HEAD 2>/dev/null || true)
    if [ "${head}" != "${baseline}" ]; then
        say "vcpkg：当前 HEAD ${head:-未知} 不是 baseline，切回 ${baseline} ..."
        git -C "${root}" fetch --depth 1 origin "${baseline}"
        git -C "${root}" checkout -q --detach FETCH_HEAD
    fi
}

# 版本判定复用门禁自己的实现：CI 说的「版本对」就是本机钩子说的同一个「对」。
verify_toolchain() {
    local prefix
    prefix=$(clang_prefix)
    [ -n "${prefix}" ] || die "从 scripts/checks/lib.sh 里读不到 PRISM_CLANG_VERSION_PREFIX。"

    export CLANG_ROOT="${LLVM_BIN}"
    # shellcheck source=../../.githooks/../scripts/checks/lib.sh disable=SC1091
    . "${PROJECT_ROOT}/scripts/checks/lib.sh"

    local tool
    for tool in clang clang++ clang-format clang-tidy; do
        prism_require_tool "${tool}" > /dev/null || die "${tool} 不是 ${prefix}.x，拒绝在版本不符的 runner 上跑门禁。"
    done

    # 预设调用裸 clang，而 runner 自带的 18 就排在 PATH 上，所以要解析一次「下一步真正会用的那个」。
    local resolved
    resolved=$(PATH="${LLVM_BIN}:${PATH}" command -v clang 2>/dev/null || true)
    [ -n "${resolved}" ] || die "PATH 上找不到 clang。"
    case "$(PATH="${LLVM_BIN}:${PATH}" clang --version 2>/dev/null | head -n 1)" in
        *"clang version ${prefix}"*) : ;;
        *) die "PATH 上的 clang（${resolved}）不是 ${prefix}.x —— CMakePresets.json 的 linux-base 用的是裸 clang。" ;;
    esac
    say "PATH 工具链校验通过：${resolved} → $(PATH="${LLVM_BIN}:${PATH}" clang --version | head -n 1)"
    say "工具链版本校验通过：clang / clang++ / clang-format / clang-tidy 全部 ${prefix}.x（${LLVM_BIN}）"
}

main() {
    if [ "${1:-}" = "--clang-prefix" ]; then
        clang_prefix
        exit 0
    fi
    if [ "${1:-}" = "--vcpkg-baseline" ]; then
        vcpkg_baseline
        exit 0
    fi

    local components=("$@")
    [ "${#components[@]}" -gt 0 ] || components=(llvm)

    local component
    for component in "${components[@]}"; do
        case "${component}" in
            llvm) install_llvm ;;
            cmake) install_cmake ;;
            system) install_system_deps ;;
            vcpkg) install_vcpkg ;;
            *) die "未知组件 '${component}'（可选：llvm cmake system vcpkg）" ;;
        esac
    done

    # PATH 必须在校验之前就摆好：校验要检查的正是「下一个步骤会用的那个」clang。
    case " ${components[*]} " in
        *" llvm "*) prepend_llvm_path ;;
    esac

    verify_toolchain
}

main "$@"
