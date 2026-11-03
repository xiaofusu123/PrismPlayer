## Summary

<!-- 改了什么、为什么；关联 issue 用 Closes #<n> -->

## Standards

- [ ] 设计原则：写明 SOLID / DRY / KISS / YAGNI 的取舍；没有结构变化写“不适用”，不要空着
- [ ] API 边界：HTTP/JSON → 更新 `docs/openapi/openapi.yaml`；WebSocket/异步消息 → 更新 `docs/asyncapi/asyncapi.yaml`；公开 C 接口 → `Player.h` / `Types.h` 能按 C17 编译
- [ ] 契约检查：`sh scripts/checks/contracts.sh` 通过（CI 的 `checks-self-test` job 也会跑同一入口）

## Evidence

<!-- 命令 + 结果；本地与 CI 证据分别写 -->

## Merge Danger

<!-- 门禁是否被削弱、破坏半径、回滚方式；没有就写“无” -->
