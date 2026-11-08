-- 项目计划书.md §9.4 五条查询的 EXPLAIN 验收：每条查询的 type 不得出现 ALL（全表扫）。
-- 由 scripts/verify-db-queries.sh 执行，断言输出中不含 "access_type": "ALL"。
-- 查询用字面量（'r1'、1、日期）以便直接 EXPLAIN，不依赖真实数据。

-- 查询 1：判断用户是否已在房间内 —— 走 ROOM_MEMBERS(room_id, user_id) 联合唯一索引（§9.4-1）
EXPLAIN FORMAT=JSON
SELECT role FROM ROOM_MEMBERS WHERE room_id = 'r1' AND user_id = 1;

-- 查询 2：拉取房间最近 N 条消息 —— 走 MESSAGES(room_id, sent_at) 联合索引（§9.4-1）
EXPLAIN FORMAT=JSON
SELECT id, sender_id, content, sent_at
FROM MESSAGES
WHERE room_id = 'r1'
ORDER BY sent_at DESC
LIMIT 20;

-- 查询 3：房间列表页（房间 + 成员数 + 房主昵称）—— 一次 JOIN + GROUP BY，避免 N+1（§9.4-2）
--         ROOMS 走 idx_rooms_list 覆盖索引；u.nickname 由 u.id（PK）函数依赖，满足 ONLY_FULL_GROUP_BY。
EXPLAIN FORMAT=JSON
SELECT r.room_id, r.name, r.state, r.created_at,
       u.nickname AS owner_nickname,
       COUNT(m.user_id) AS member_count
FROM ROOMS r
JOIN USERS u ON u.id = r.owner_id
LEFT JOIN ROOM_MEMBERS m ON m.room_id = r.room_id
GROUP BY r.room_id, r.name, r.state, r.created_at, r.owner_id;

-- 查询 4：消息列表键集分页（sent_at 游标，避免 LIMIT offset）—— 走 MESSAGES(room_id, sent_at)（§9.4-4）
--         注意：列表查询只取需要的列，content（TEXT）不进列表页（§9.4-3）。
EXPLAIN FORMAT=JSON
SELECT id, sender_id, sent_at
FROM MESSAGES
WHERE room_id = 'r1' AND sent_at < '2026-11-01 00:00:00'
ORDER BY sent_at DESC
LIMIT 20;

-- 查询 5：写路径 —— 房间状态变更走 uk_rooms_room_id 唯一索引定位（§9.4-5）
--         事务包裹「更新房间 + 写成员表」见下注释；EXPLAIN 校验 UPDATE 的查找路径不含全表扫。
-- START TRANSACTION;
--   UPDATE ROOMS SET state = 'closed' WHERE room_id = 'r1';
--   INSERT INTO ROOM_MEMBERS (room_id, user_id) VALUES ('r1', 2);
-- COMMIT;
EXPLAIN FORMAT=JSON
UPDATE ROOMS SET state = 'closed' WHERE room_id = 'r1';
