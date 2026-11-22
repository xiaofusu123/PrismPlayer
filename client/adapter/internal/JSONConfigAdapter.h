#pragma once

#include "ConfigAdapter.h"

#include <nlohmann/json.hpp>

namespace Prism::Adapter {

/**
 * @class JSONConfigAdapter
 * @brief 基于 nlohmann-json 的 JSON 配置文件读写实现
 *
 * - 键名支持 `.` 分隔的层级路径（如 `audio.sample_rate`），写入时自动创建中间对象。
 * - 类型化读取按目标类型判定，类型不符返回 nullopt 而不是强转；唯一例外是
 *   get_double()，它接受 JSON 里写成整数的数值（`60` 视同 `60.0`），
 *   免得写配置的人因为漏了一个小数点就读不出值。
 * - 配置树根节点始终是一个 JSON object，dump 时用 4 空格缩进。
 * - **原子写**：序列化先在内存完成，再写 `<target>.tmp-<时间>-<随机数>`，
 *   flush + close 都成功后才 `rename` 替换目标；目标文件全程不被打开，任何时刻
 *   中断都只会留下完整的旧内容或完整的新内容。所有失败路径都会清掉临时文件。
 * - **当前目标**：`file_path_` 只在 `load()` / `save_as()` 成功时更新；`save_as()`
 *   成功即把后续 `save()` 重定向到新路径，失败保留原目标。
 * - **不抛异常**：dump()/parse()/filesystem 的异常都在实现内部转成 false / nullopt。
 */
class JSONConfigAdapter : public ConfigAdapter {
  public:
    JSONConfigAdapter() = default;
    ~JSONConfigAdapter() override = default;

    bool load(const std::string& file_path) override;
    bool save() override;
    bool save_as(const std::string& file_path) override;

    std::optional<std::string> get_string(const std::string& key) override;
    std::optional<int64_t> get_int(const std::string& key) override;
    std::optional<double> get_double(const std::string& key) override;
    std::optional<bool> get_bool(const std::string& key) override;

    void set_string(const std::string& key, const std::string& value) override;
    void set_int(const std::string& key, int64_t value) override;
    void set_double(const std::string& key, double value) override;
    void set_bool(const std::string& key, bool value) override;

    bool has(const std::string& key) override;

  private:
    nlohmann::json root_ = nlohmann::json::object(); /**< 已解析的配置树，根节点恒为 object */
    std::string file_path_;                          /**< 当前配置文件路径，未加载时为空 */
};

} // namespace Prism::Adapter
