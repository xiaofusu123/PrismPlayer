-- PrismPlayer 服务端 MySQL 表结构（项目计划书.md §9.2 / §9.3 E-R 图的落地）
--
-- 与 server/adapter/include/DBAdapter.h 的映射（内存/MySQL 两实现共用同一接口）：
--   UserRecord.username   -> USERS.account（登录账号，唯一键）
--   UserRecord.password_hash -> USERS.password_hash
--   UserRecord.salt       -> USERS.salt（CryptoAdapter 的 SHA-256 + 随机盐需要单独一列，
--                             E-R 图未画，属实现落地时的必要补充）
--   RoomRecord.room_id    -> ROOMS.room_id（字符串自然键；E-R 图的 bigint id 在此为代理键）
--   RoomRecord.name/owner_id -> ROOMS.name / ROOMS.owner_id
--   RoomRecord.max_members -> 无对应列（E-R 图用 state 表达房间状态，本接口不落地该字段）
--   RoomMemberRecord.room_id/user_id -> ROOM_MEMBERS.room_id / user_id
--
-- 索引依据项目计划书.md §9.4：ROOM_MEMBERS(room_id, user_id) 联合唯一索引、
-- MESSAGES(room_id, sent_at) 联合索引；idx_rooms_list 支撑「房间列表页」一次 JOIN + GROUP BY
-- 时 ROOMS 走覆盖索引（避免全表扫）。

CREATE TABLE IF NOT EXISTS USERS (
    id BIGINT NOT NULL AUTO_INCREMENT,
    account VARCHAR(64) NOT NULL,
    nickname VARCHAR(64) NOT NULL DEFAULT '',
    email VARCHAR(128) NULL,
    password_hash VARCHAR(128) NOT NULL,
    salt VARCHAR(64) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_users_account (account),
    UNIQUE KEY uk_users_email (email)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

CREATE TABLE IF NOT EXISTS ROOMS (
    id BIGINT NOT NULL AUTO_INCREMENT,
    room_id VARCHAR(64) NOT NULL,
    name VARCHAR(128) NOT NULL,
    owner_id BIGINT NOT NULL,
    state VARCHAR(32) NOT NULL DEFAULT 'open',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_rooms_room_id (room_id),
    KEY idx_rooms_list (room_id, name, state, created_at, owner_id)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

CREATE TABLE IF NOT EXISTS ROOM_MEMBERS (
    room_id VARCHAR(64) NOT NULL,
    user_id BIGINT NOT NULL,
    role VARCHAR(32) NOT NULL DEFAULT 'member',
    joined_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (room_id, user_id)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

CREATE TABLE IF NOT EXISTS MESSAGES (
    id BIGINT NOT NULL AUTO_INCREMENT,
    room_id VARCHAR(64) NOT NULL,
    sender_id BIGINT NOT NULL,
    content TEXT NOT NULL,
    sent_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_messages_room_sent (room_id, sent_at)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

CREATE TABLE IF NOT EXISTS FILE_SHARES (
    id BIGINT NOT NULL AUTO_INCREMENT,
    room_id VARCHAR(64) NOT NULL,
    uploader_id BIGINT NOT NULL,
    filename VARCHAR(255) NOT NULL,
    size BIGINT NOT NULL DEFAULT 0,
    checksum VARCHAR(128) NOT NULL DEFAULT '',
    shared_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_file_shares_room (room_id)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;
