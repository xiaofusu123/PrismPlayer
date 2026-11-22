#!/usr/bin/env bash
# 生成 ui/lib/ffi/ffi_bridge.dart（WSL / Linux）
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ui_dir="$repo_root/ui"
[ -f "$ui_dir/pubspec.yaml" ] || {
  echo "找不到 ui/pubspec.yaml（仓库根：$repo_root）" >&2
  exit 1
}

clang_bin=""
if [ -n "${CLANG_ROOT:-}" ] && [ -x "${CLANG_ROOT}/clang" ]; then
  clang_bin="${CLANG_ROOT}/clang"
else
  clang_bin="$(command -v clang || true)"
fi
[ -n "$clang_bin" ] || {
  echo "找不到 clang：设置 CLANG_ROOT 或把 clang 放进 PATH" >&2
  exit 1
}

find_libclang() {
  local c
  for c in \
    "${CLANG_ROOT:+$CLANG_ROOT/libclang.so}" \
    "${CLANG_ROOT:+$CLANG_ROOT/../lib/libclang.so}" \
    "$("$clang_bin" -print-file-name=libclang.so 2>/dev/null || true)" \
    /usr/lib/llvm-*/lib/libclang.so* \
    /usr/lib/x86_64-linux-gnu/libclang.so*; do
    if [ -n "$c" ] && [ -f "$c" ]; then
      printf '%s\n' "$c"
      return 0
    fi
  done
  return 1
}

libclang="$(find_libclang)" || {
  echo "找不到 libclang.so：设置 CLANG_ROOT，或安装 llvm-dev / libclang-dev" >&2
  exit 1
}

resource_include="$("$clang_bin" -print-resource-dir)/include"
[ -d "$resource_include" ] || {
  echo "找不到 clang 资源目录：$resource_include" >&2
  exit 1
}

yaml="$ui_dir/ffigen.yaml"
[ -f "$yaml" ] || { echo "缺少配置文件：$yaml" >&2; exit 1; }

tmp_yaml="$(mktemp "$ui_dir/.ffigen.generated.XXXXXX")"
trap 'rm -f "$tmp_yaml"' EXIT

{
  cat "$yaml"
  printf '\n# 以下由 ffigen.sh 注入\nllvm-path:\n  - %s\n' "$libclang"
} > "$tmp_yaml"

cd "$ui_dir"
dart run ffigen --config "$(basename "$tmp_yaml")" \
  --compiler-opts "-isystem $resource_include"

echo "已生成 $ui_dir/lib/ffi/ffi_bridge.dart"