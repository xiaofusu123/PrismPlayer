#pragma once

#include "ConfigAdapter.h"

namespace Prism::Adapter {

/**
 * @class JSONConfigAdapter
 * @brief 基于 nlohmann-json 的 JSON 配置文件读写实现
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
    // TODO: 添加 nlohmann::json 对象及当前文件路径等私有成员
};

} // namespace Prism::Adapter
