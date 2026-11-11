-- ============================================================================
-- PrismPlayer 服务端 MySQL 表结构（项目计划书.md §9.2 / §9.3）
--
-- 本文件是服务端 MySQL 的 DDL 唯一真源；字段与 `server/adapter/include/DBAdapter.h`
-- 的 UserRecord / RoomRecord / RoomMemberRecord 对齐，其余表（messages / file_shares）
-- 补齐 §9.2 列出的四张表与 §9.3 E-R 图。
--
-- 引擎与字符集：InnoDB + utf8mb4（用户名、房间名、聊天内容都可能是非 ASCII）。
-- ============================================================================

-- 用户表（集中认证）
CREATE TABLE IF NOT EXISTS users (
    id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    username      VARCHAR(64)     NOT NULL,
    password_hash VARCHAR(128)    NOT NULL,
    salt          VARCHAR(64)     NOT NULL,
    created_at    DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_users_username (username)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

-- 房间表（房间状态）
-- state: idle / playing / paused；current_media 与 position_ms 承载「当前媒体与位置」。
CREATE TABLE IF NOT EXISTS rooms (
    room_id      VARCHAR(64)   NOT NULL,
    name         VARCHAR(128)  NOT NULL,
    owner_id     BIGINT UNSIGNED NOT NULL,
    max_members  INT           NOT NULL DEFAULT 0,
    state        VARCHAR(16)   NOT NULL DEFAULT 'idle',
    current_media VARCHAR(512) NOT NULL DEFAULT '',
    position_ms  BIGINT        NOT NULL DEFAULT 0,
    created_at   DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (room_id),
    KEY idx_rooms_owner (owner_id)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

-- 房间成员表
-- 联合唯一索引 (room_id, user_id) 同时作为「是否已在房间内」的判据（§9.4 第 1 条）。
CREATE TABLE IF NOT EXISTS room_members (
    room_id   VARCHAR(64)      NOT NULL,
    user_id   BIGINT UNSIGNED  NOT NULL,
    joined_at DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (room_id, user_id),
    KEY idx_room_members_user (user_id)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

-- 消息表（聊天记录）
-- 联合索引 (room_id, sent_at) 支撑「拉取房间最近 N 条消息」（§9.4 第 1 条）。
CREATE TABLE IF NOT EXISTS messages (
    id        BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    room_id   VARCHAR(64)     NOT NULL,
    sender_id BIGINT UNSIGNED NOT NULL,
    content   TEXT            NOT NULL,
    sent_at   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_messages_room_sent (room_id, sent_at)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

-- 文件分享记录表
CREATE TABLE IF NOT EXISTS file_shares (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    room_id     VARCHAR(64)     NOT NULL,
    uploader_id BIGINT UNSIGNED NOT NULL,
    filename    VARCHAR(256)    NOT NULL,
    size        BIGINT          NOT NULL DEFAULT 0,
    checksum    VARCHAR(64)     NOT NULL,
    shared_at   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_file_shares_room (room_id)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;
