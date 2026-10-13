#!/usr/bin/env bash

set -uo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
PREREQUISITE_DOC="docs/markdown/项目框架.md 7.1（环境与依赖基线）"

CHECKS=0
FAILURES=0

check() {
    CHECKS=$((CHECKS + 1))
    if [ "$1" -eq 1 ]; then
        echo "  [PASS] $2"
    else
        FAILURES=$((FAILURES + 1))
        echo "  [FAIL] $2"
    fi
}

test_config() {
    name=$1
    dir=$2
    triplet=$3
    min_pkgs=$4
    shift 4

    echo
    echo "=== 预设 ${name}（${dir}） ==="
    if [ ! -d "${dir}" ]; then
        check 0 "构建目录存在：${dir}"
        return
    fi
    check 1 "构建目录存在：${dir}"

    status="${dir}/vcpkg_installed/vcpkg/status"
    pkgs=0
    if [ -f "${status}" ]; then
        pkgs=$(grep -c '^Package: ' "${status}")
    fi
    if [ -f "${status}" ]; then check 1 "依赖安装树状态文件存在：vcpkg_installed/vcpkg/status"; else check 0 "依赖安装树状态文件存在：vcpkg_installed/vcpkg/status"; fi
    if [ "${pkgs}" -ge "${min_pkgs}" ]; then check 1 "安装树里有 ${pkgs} 个已安装包（阈值 ${min_pkgs}）"; else check 0 "安装树里有 ${pkgs} 个已安装包（阈值 ${min_pkgs}）"; fi

    lib_dir="${dir}/vcpkg_installed/${triplet}/lib"
    libs=0
    if [ -d "${lib_dir}" ]; then
        libs=$(find "${lib_dir}" -type f \( -name '*.a' -o -name '*.lib' -o -name '*.so' -o -name '*.so.*' \) | wc -l)
    fi
    if [ "${libs}" -ge 10 ]; then check 1 "安装树里有 ${libs} 个库/导入库文件（阈值 10，位于 ${triplet}/lib）"; else check 0 "安装树里有 ${libs} 个库/导入库文件（阈值 10，位于 ${triplet}/lib）"; fi

    while [ $# -gt 0 ]; do
        art="${1%%:*}"
        thr="${1##*:}"
        shift
        path="${dir}/${art}"
        if [ ! -f "${path}" ]; then
            check 0 "产物存在：${art}（缺失）"
            continue
        fi
        size=$(stat -c %s "${path}")
        if [ "${size}" -ge "${thr}" ]; then
            check 1 "产物 ${art} = ${size} B（阈值 ${thr} B）"
        else
            check 0 "产物 ${art} = ${size} B（阈值 ${thr} B）"
        fi
    done

    svc="${dir}/client/service/libclient-service.so"
    if ! command -v objdump >/dev/null 2>&1; then
        echo "  [SKIP] 未找到 objdump，跳过导出/导入检查"
        return
    fi
    if [ ! -f "${svc}" ]; then
        check 0 "服务动态库存在，可检查导出：client/service/libclient-service.so"
        return
    fi

    dump_p=$(objdump -p "${svc}" 2>/dev/null)
    dump_t=$(objdump -T "${svc}" 2>/dev/null)
    exports=$(printf '%s\n' "${dump_t}" | grep -c 'prism_player_')
    if [ "${exports}" -ge 32 ]; then check 1 "服务动态库导出 ${exports} 个 prism_player_* 符号（阈值 32）"; else check 0 "服务动态库导出 ${exports} 个 prism_player_* 符号（阈值 32）"; fi
    abi=$(printf '%s\n' "${dump_p}" | grep -c 'NEEDED.*libc++')
    if [ "${abi}" -eq 0 ]; then check 1 "服务动态库未依赖 libc++（跨标准库 ABI 分裂会让链接失败，见 ${PREREQUISITE_DOC}）"; else check 0 "服务动态库依赖了 libc++（见 ${PREREQUISITE_DOC}）"; fi
}

selected=()
for arg in "$@"; do
    case "${arg}" in
        1) selected+=("debug");;
        2) selected+=("release");;
    esac
done
if [ "${#selected[@]}" -eq 0 ]; then
    selected=("debug" "release")
fi

echo "构建产物验收（#66 AC4）：平台 linux，检查 $(IFS='、'; echo "${selected[*]}")"
echo "体积阈值刻意高于空归档/占位文件，并低于实测产物体积；实测值见 ${PREREQUISITE_DOC}"

for config in "${selected[@]}"; do
    case "${config}" in
        debug)
            test_config "linux:debug" "${PROJECT_ROOT}/build/Linux/debug" x64-linux 25 \
                client/engine/libclient-engine.so:36000 \
                client/service/libclient-service.so:2000000 \
                client/business/av_sync/libclient-av_sync.a:6000000 \
                server/Prism-server:4000
            ;;
        release)
            test_config "linux:release" "${PROJECT_ROOT}/build/Linux/release" x64-linux 25 \
                client/engine/libclient-engine.so:6000 \
                client/service/libclient-service.so:120000 \
                client/business/av_sync/libclient-av_sync.a:490000 \
                server/Prism-server:4000
            ;;
    esac
done

echo
if [ "${FAILURES}" -eq 0 ]; then
    echo "验收通过：${CHECKS} 项检查全部通过（平台 linux）"
    exit 0
else
    echo "验收失败：${FAILURES} / ${CHECKS} 项不通过（平台 linux）"
    exit 1
fi
