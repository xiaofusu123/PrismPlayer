#pragma once

#include "SyncTypes.h"

namespace Prism::Business {

/**
 * @class IPlaybackStateMachine
 * @brief 播放状态机抽象接口
 *
 * 控制播放/暂停/Seek 状态转换，校验转换合法性。
 * 状态流转：
 *   UNINIT → CALIBATING → SYNCHRONIZED ⇄ AHEAD/BEHIND
 *   DISABLE（单独播放模式）/ SYNC_ERROR（错误）
 */
class IPlaybackStateMachine {
public:
    virtual ~IPlaybackStateMachine() = default;

    /**
     * @brief 状态转换
     * @param target 目标状态
     * @return 转换成功返回 true，非法转换返回 false
     */
    virtual bool transition(SyncState target) = 0;

    /**
     * @brief 获取当前同步状态
     * @return SyncState 当前状态
     */
    virtual SyncState get_state() const = 0;

    /**
     * @brief 当前状态是否允许播放
     * @return true 允许播放
     */
    virtual bool can_play() const = 0;

    /**
     * @brief 当前状态是否允许跳转
     * @return true 允许跳转
     */
    virtual bool can_seek() const = 0;

    /**
     * @brief 重置状态机到 UNINIT
     */
    virtual void reset() = 0;
};

} // namespace Prism::Business
