#!/bin/sh
# 验证 项目计划书.md §9.4 的查询优化要求：对 server/adapter/verify_queries.sql 里每条 EXPLAIN
# 断言 type 不出现 ALL（全表扫）。只依赖 mysql 客户端，连接参数走 PRISM_MYSQL_*（与 main.cpp 一致）。
# 用法：
#   PRISM_MYSQL_HOST=127.0.0.1 PRISM_MYSQL_USER=root PRISM_MYSQL_PASSWORD=... \
#       PRISM_MYSQL_DATABASE=prism sh scripts/verify-db-queries.sh

set -eu

HOST=${PRISM_MYSQL_HOST:-127.0.0.1}
PORT=${PRISM_MYSQL_PORT:-3306}
USER=${PRISM_MYSQL_USER:-root}
PASSWORD=${PRISM_MYSQL_PASSWORD:-}
DATABASE=${PRISM_MYSQL_DATABASE:-prism}

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)
SCHEMA_FILE="${ROOT_DIR}/server/adapter/schema.sql"
QUERIES_FILE="${ROOT_DIR}/server/adapter/verify_queries.sql"

if ! command -v mysql >/dev/null 2>&1; then
    echo "错误：找不到 mysql 客户端，请先安装 MySQL 客户端工具。" >&2
    exit 1
fi

export MYSQL_PWD="${PASSWORD}"

run_mysql() {
    mysql -h "${HOST}" -P "${PORT}" -u "${USER}" --batch --raw --skip-column-names "$@"
}

echo "==> 应用表结构：${SCHEMA_FILE}"
if ! run_mysql "${DATABASE}" < "${SCHEMA_FILE}"; then
    echo "错误：应用 schema.sql 失败。" >&2
    exit 1
fi

echo "==> 执行 EXPLAIN：${QUERIES_FILE}"
OUTPUT=$(run_mysql "${DATABASE}" < "${QUERIES_FILE}" 2>&1) || {
    echo "错误：执行 verify_queries.sql 失败：" >&2
    printf '%s\n' "${OUTPUT}" >&2
    exit 1
}

if printf '%s\n' "${OUTPUT}" | grep -q '"access_type": "ALL"'; then
    echo "失败：检测到全表扫（type = ALL）。" >&2
    printf '%s\n' "${OUTPUT}" | grep -n '"access_type": "ALL"' >&2
    exit 1
fi

echo "通过：所有查询均走索引，type != ALL。"
exit 0
