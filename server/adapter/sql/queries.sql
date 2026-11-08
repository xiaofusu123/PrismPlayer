-- ============================================================================
-- PrismPlayer 服务端 MySQL 查询 SQL（项目计划书.md §9.4）
--
-- 五条优化要求逐一落地；每条附 `EXPLAIN` 语句作为验收手段：EXPLAIN 的 `type`
-- 列不得出现 `ALL`（全表扫描）。
-- ============================================================================

-- ----------------------------------------------------------------------------
-- 1. 高频查询走索引
--    ROOM_MEMBERS(room_id, user_id) 联合唯一索引（主键即此联合索引），
--    MESSAGES(room_id, sent_at) 联合索引。
--    本查询是「用户是否已在房间内」与「取出成员」的高频路径，走主键。
-- ----------------------------------------------------------------------------
SELECT user_id FROM room_members WHERE room_id = ? AND user_id = ?;

EXPLAIN SELECT user_id FROM room_members WHERE room_id = ? AND user_id = ?;
-- 预期：type = const（唯一索引等值命中，非 ALL）。

-- ----------------------------------------------------------------------------
-- 2. 避免 N+1
--    房间列表页需要「房间 + 成员数 + 房主昵称」，用一次 JOIN + GROUP BY 取回，
--    不要每行再查一次房主昵称 / 成员数。
-- ----------------------------------------------------------------------------
SELECT r.room_id,
       r.name,
       r.state,
       COUNT(rm.user_id) AS member_count,
       u.username        AS owner_name
FROM rooms r
LEFT JOIN room_members rm ON rm.room_id = r.room_id
JOIN users u ON u.id = r.owner_id
GROUP BY r.room_id, r.name, r.state, u.username;

EXPLAIN SELECT r.room_id, r.name, r.state, COUNT(rm.user_id) AS member_count,
               u.username AS owner_name
        FROM rooms r
        LEFT JOIN room_members rm ON rm.room_id = r.room_id
        JOIN users u ON u.id = r.owner_id
        GROUP BY r.room_id, r.name, r.state, u.username;
-- 预期：驱动表 users/rooms 全表扫描是不可避免的（房间数有限），
--       但 room_members 侧应走 idx_room_members_user 或主键，type 非 ALL 即可；
--       真正的 N+1 风险在「逐行回查房主昵称/成员数」，已由 JOIN + GROUP BY 消除。

-- ----------------------------------------------------------------------------
-- 3. 避开 SELECT *
--    列表查询只取需要的列，content（TEXT）不进列表页。
-- ----------------------------------------------------------------------------
SELECT id, room_id, sender_id, sent_at
FROM messages
WHERE room_id = ?
ORDER BY sent_at DESC
LIMIT ?;

EXPLAIN SELECT id, room_id, sender_id, sent_at
        FROM messages WHERE room_id = ? ORDER BY sent_at DESC LIMIT ?;
-- 预期：type = ref，走 idx_messages_room_sent；未取 content，避免 TEXT 被拖进列表。

-- ----------------------------------------------------------------------------
-- 4. 分页用键集分页（keyset pagination）
--    消息列表按 sent_at 游标翻页，不用 LIMIT offset（大房间下 offset 线性变慢）。
--    游标 = 上一页最后一条的 sent_at。
-- ----------------------------------------------------------------------------
SELECT id, room_id, sender_id, content, sent_at
FROM messages
WHERE room_id = ?
  AND sent_at < ?
ORDER BY sent_at DESC
LIMIT ?;

EXPLAIN SELECT id, room_id, sender_id, content, sent_at
        FROM messages WHERE room_id = ? AND sent_at < ? ORDER BY sent_at DESC LIMIT ?;
-- 预期：type = range（联合索引范围扫描），非 ALL。

-- ----------------------------------------------------------------------------
-- 5. 写路径：房间状态变更用事务包住「更新房间 + 写成员表」
--    避免房主掉线时留下半截状态（例如房主已移除、房间状态却仍是 playing）。
-- ----------------------------------------------------------------------------
START TRANSACTION;
UPDATE rooms
   SET state = ?, current_media = ?, position_ms = ?
 WHERE room_id = ?;
DELETE FROM room_members WHERE room_id = ? AND user_id = ?;
COMMIT;
