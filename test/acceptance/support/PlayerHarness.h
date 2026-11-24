#pragma once

#include "Player.h"

#include "FakeEngines.h"

#include <cstddef>
#include <memory>
#include <vector>

/**
 * @file PlayerHarness.h
 * @brief 验收用例共用的 ABI 辅助：事件收集与句柄 RAII。
 *
 * 只依赖 `client/service/include/Player.h`（公开 C ABI）与验收侧自己的假引擎，
 * 不 include 任何 `internal/` 头（#87 决策 2 / #88 AC）。
 */

namespace Prism::Qa {

/** @brief 模式 A 打开的合成媒体地址：不落地真实文件，只用来穿过 ABI 的 open 路径。 */
inline constexpr const char* FAKE_MEDIA_URI = "acceptance://synthetic.mp4";

/**
 * @class EventLog
 * @brief 把 ABI 事件回调按序收进缓冲，供“事件子序列”断言。
 */
class EventLog {
  public:
    static void callback(PrismEventType type, const void* data, void* user_data);

    [[nodiscard]] bool contains(PrismEventType type) const;
    [[nodiscard]] std::size_t count(PrismEventType type) const;
    [[nodiscard]] const std::vector<PrismEventType>& events() const { return events_; }

  private:
    std::vector<PrismEventType> events_;
};

/**
 * @class PlayerGuard
 * @brief `prism_player_create` / `prism_player_destroy` 的 RAII 包装。
 *
 * 断言失败提前返回时也不会泄漏句柄；`get()` 拿到原始句柄给 ABI 用。
 */
class PlayerGuard {
  public:
    explicit PlayerGuard(const PrismConfig* config = nullptr, PrismEventCallback callback = nullptr,
                         void* user_data = nullptr);
    ~PlayerGuard();

    PlayerGuard(const PlayerGuard&) = delete;
    PlayerGuard& operator=(const PlayerGuard&) = delete;

    [[nodiscard]] PrismPlayerHandle get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

  private:
    PrismPlayerHandle handle_{nullptr};
};

/**
 * @struct FakeBackends
 * @brief 一对记录型假引擎与它们的工厂，生命周期覆盖被测播放器。
 */
struct FakeBackends {
    FakeBackends();

    std::shared_ptr<EngineRecorder> audio;
    std::shared_ptr<EngineRecorder> video;
    FakeAudioFactory audio_factory;
    FakeVideoFactory video_factory;
};

} // namespace Prism::Qa
