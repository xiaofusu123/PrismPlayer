#!/bin/sh
# 契约检查：公开 C 头文件按 C17 编译；AsyncAPI / OpenAPI 契约可校验。
# 用法：sh scripts/checks/contracts.sh
# 退出码：0 全部通过；1 有契约不成立。
set -u

CONTRACTS_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
. "$CONTRACTS_ROOT/scripts/checks/lib.sh"

CONTRACTS_TMP=$(prism_tmpdir)
trap 'rm -rf "$CONTRACTS_TMP"' EXIT

CONTRACTS_FAILED=0

contracts_ok() {
    printf '[契约] 通过：%s\n' "$1"
}

contracts_bad() {
    printf '[契约] 失败：%s\n' "$1" >&2
    CONTRACTS_FAILED=1
}

# C ABI：公开头文件必须能按 C17 编译（不要求链接，只做语法/类型检查）。
check_c_abi() {
    cc_bin=${CC:-}
    if [ -z "$cc_bin" ]; then
        for candidate in cc clang gcc; do
            if command -v "$candidate" >/dev/null 2>&1; then
                cc_bin=$candidate
                break
            fi
        done
    fi
    if [ -z "$cc_bin" ] && command -v prism_find_tool >/dev/null 2>&1; then
        cc_bin=$(prism_find_tool clang 2>/dev/null || true)
    fi
    if [ -z "$cc_bin" ]; then
        contracts_bad "找不到 C 编译器（cc/clang/gcc），无法检查 C ABI 的 C17 兼容性"
        return
    fi

    printf '#include "Player.h"\nint main(void) { return 0; }\n' > "$CONTRACTS_TMP/cabi_check.c"
    printf '[契约] C ABI：%s -std=c17 编译 Player.h ...\n' "$cc_bin"
    if "$cc_bin" -std=c17 -fsyntax-only \
        -I "$CONTRACTS_ROOT/client/common" \
        -I "$CONTRACTS_ROOT/client/service/include" \
        "$CONTRACTS_TMP/cabi_check.c"; then
        contracts_ok "Player.h / Types.h 可按 C17 编译"
    else
        contracts_bad "公开 C 头文件不能按 C17 编译（见上方诊断；应改用 stdint/stdbool/stddef 与 C 兼容的聚合初始化）"
    fi
}

# 契约文件：存在就必须能通过对应 lint；OpenAPI 在首个 HTTP 接口落地前允许缺省。
check_asyncapi() {
    spec="$CONTRACTS_ROOT/docs/asyncapi/asyncapi.yaml"
    if [ ! -f "$spec" ]; then
        contracts_bad "缺少 docs/asyncapi/asyncapi.yaml（WebSocket/异步消息契约）"
        return
    fi
    if ! command -v npx >/dev/null 2>&1; then
        if [ "${CI:-}" = "true" ]; then
            contracts_bad "CI 环境找不到 npx，无法校验 AsyncAPI 契约"
        else
            printf '[契约] 跳过：未找到 npx（本机未装 Node.js；CI 会执行 AsyncAPI lint）\n'
        fi
        return
    fi
    printf '[契约] AsyncAPI：校验 docs/asyncapi/asyncapi.yaml ...\n'
    if npx --yes @asyncapi/cli@3 validate "$spec"; then
        contracts_ok "AsyncAPI 契约可校验"
    else
        contracts_bad "AsyncAPI 契约校验未通过"
    fi
}

check_openapi() {
    spec="$CONTRACTS_ROOT/docs/openapi/openapi.yaml"
    if [ ! -f "$spec" ]; then
        printf '[契约] 跳过：docs/openapi/openapi.yaml 尚未创建（首个 HTTP 接口落地时创建）\n'
        return
    fi
    if ! command -v npx >/dev/null 2>&1; then
        if [ "${CI:-}" = "true" ]; then
            contracts_bad "CI 环境找不到 npx，无法校验 OpenAPI 契约"
        else
            printf '[契约] 跳过：未找到 npx（本机未装 Node.js；CI 会执行 OpenAPI lint）\n'
        fi
        return
    fi
    printf '[契约] OpenAPI：校验 docs/openapi/openapi.yaml ...\n'
    if npx --yes @redocly/cli@1 lint "$spec"; then
        contracts_ok "OpenAPI 契约可校验"
    else
        contracts_bad "OpenAPI 契约校验未通过"
    fi
}

check_c_abi
check_asyncapi
check_openapi

if [ "$CONTRACTS_FAILED" -ne 0 ]; then
    printf '[契约] 有检查未通过。\n' >&2
    exit 1
fi
printf '[契约] 全部通过。\n'
