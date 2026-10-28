#include "JSONConfigAdapter.h"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

namespace Prism::Adapter {
namespace {

/**
 * @brief 按 `.` 切分配置键，得到层级路径
 * @param key 配置键名，如 `audio.sample_rate`
 * @return 各级键名；`key` 为空时返回单个空串，由调用方判为非法
 */
std::vector<std::string> split_key(const std::string& key) {
    std::vector<std::string> parts;
    std::string part;
    for (char ch : key) {
        if (ch == '.') {
            parts.push_back(part);
            part.clear();
        } else {
            part.push_back(ch);
        }
    }
    parts.push_back(part);
    return parts;
}

/**
 * @brief 在配置树里按键查找节点
 * @param root 配置树根节点
 * @param key 配置键名
 * @return 命中返回节点指针，未命中或 key 非法返回 nullptr
 */
const nlohmann::json* find_node(const nlohmann::json& root, const std::string& key) {
    if (key.empty()) {
        return nullptr;
    }

    const nlohmann::json* node = &root;
    for (const std::string& part : split_key(key)) {
        if (part.empty() || !node->is_object()) {
            return nullptr;
        }

        auto it = node->find(part);
        if (it == node->end()) {
            return nullptr;
        }
        node = &(*it);
    }
    return node;
}

/**
 * @brief 取到键对应的节点，缺失的中间层一律补成 object
 * @param root 配置树根节点
 * @param key 配置键名
 * @return 可写的节点引用
 */
nlohmann::json& ensure_node(nlohmann::json& root, const std::string& key) {
    nlohmann::json* node = &root;
    for (const std::string& part : split_key(key)) {
        if (!node->is_object()) {
            *node = nlohmann::json::object();
        }
        node = &(*node)[part];
    }
    return *node;
}

/**
 * @brief 把配置树写到指定文件
 * @param path 目标文件路径，父目录不存在时自动创建
 * @param root 配置树
 * @return 成功返回 true
 */
bool write_json_file(const std::filesystem::path& path, const nlohmann::json& root) {
    std::error_code ec;
    std::filesystem::path parent = path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            spdlog::error("[ConfigAdapter] cannot create directory '{}': {}", parent.string(),
                          ec.message());
            return false;
        }
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("[ConfigAdapter] cannot open '{}' for writing", path.string());
        return false;
    }

    out << root.dump(4) << '\n';
    if (!out) {
        spdlog::error("[ConfigAdapter] failed to write '{}'", path.string());
        return false;
    }
    return true;
}

} // namespace

bool JSONConfigAdapter::load(const std::string& file_path) {
    if (file_path.empty()) {
        spdlog::error("[ConfigAdapter] load() called with an empty path");
        return false;
    }

    std::filesystem::path path = std::filesystem::path(file_path);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        spdlog::error("[ConfigAdapter] config file '{}' does not exist", file_path);
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        spdlog::error("[ConfigAdapter] cannot open '{}' for reading", file_path);
        return false;
    }

    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(in);
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("[ConfigAdapter] failed to parse '{}': {}", file_path, e.what());
        return false;
    }

    if (!parsed.is_object()) {
        spdlog::error("[ConfigAdapter] root of '{}' must be a JSON object", file_path);
        return false;
    }

    root_ = std::move(parsed);
    file_path_ = path.string();
    spdlog::debug("[ConfigAdapter] loaded '{}'", file_path_);
    return true;
}

bool JSONConfigAdapter::save() {
    if (file_path_.empty()) {
        spdlog::error("[ConfigAdapter] save() called before load() or save_as()");
        return false;
    }

    if (!write_json_file(std::filesystem::path(file_path_), root_)) {
        return false;
    }

    spdlog::debug("[ConfigAdapter] saved '{}'", file_path_);
    return true;
}

bool JSONConfigAdapter::save_as(const std::string& file_path) {
    if (file_path.empty()) {
        spdlog::error("[ConfigAdapter] save_as() called with an empty path");
        return false;
    }

    std::error_code ec;
    std::filesystem::path normalized =
        std::filesystem::weakly_canonical(std::filesystem::path(file_path), ec);
    if (ec) {
        spdlog::error("[ConfigAdapter] cannot normalize path '{}': {}", file_path, ec.message());
        return false;
    }

    if (!write_json_file(normalized, root_)) {
        return false;
    }

    file_path_ = normalized.string();
    spdlog::debug("[ConfigAdapter] saved as '{}'", file_path_);
    return true;
}

std::optional<std::string> JSONConfigAdapter::get_string(const std::string& key) {
    const nlohmann::json* node = find_node(root_, key);
    if (node == nullptr || !node->is_string()) {
        spdlog::debug("[ConfigAdapter] key '{}' is not a string", key);
        return std::nullopt;
    }

    return node->get<std::string>();
}

std::optional<int64_t> JSONConfigAdapter::get_int(const std::string& key) {
    const nlohmann::json* node = find_node(root_, key);
    if (node == nullptr || !node->is_number_integer()) {
        spdlog::debug("[ConfigAdapter] key '{}' is not an integer", key);
        return std::nullopt;
    }

    return node->get<int64_t>();
}

std::optional<double> JSONConfigAdapter::get_double(const std::string& key) {
    const nlohmann::json* node = find_node(root_, key);
    if (node == nullptr || !node->is_number()) {
        spdlog::debug("[ConfigAdapter] key '{}' is not a number", key);
        return std::nullopt;
    }

    return node->get<double>();
}

std::optional<bool> JSONConfigAdapter::get_bool(const std::string& key) {
    const nlohmann::json* node = find_node(root_, key);
    if (node == nullptr || !node->is_boolean()) {
        spdlog::debug("[ConfigAdapter] key '{}' is not a boolean", key);
        return std::nullopt;
    }

    return node->get<bool>();
}

void JSONConfigAdapter::set_string(const std::string& key, const std::string& value) {
    if (key.empty()) {
        spdlog::warn("[ConfigAdapter] set_string() called with an empty key");
        return;
    }

    ensure_node(root_, key) = value;
}

void JSONConfigAdapter::set_int(const std::string& key, int64_t value) {
    if (key.empty()) {
        spdlog::warn("[ConfigAdapter] set_int() called with an empty key");
        return;
    }

    ensure_node(root_, key) = value;
}

void JSONConfigAdapter::set_double(const std::string& key, double value) {
    if (key.empty()) {
        spdlog::warn("[ConfigAdapter] set_double() called with an empty key");
        return;
    }

    ensure_node(root_, key) = value;
}

void JSONConfigAdapter::set_bool(const std::string& key, bool value) {
    if (key.empty()) {
        spdlog::warn("[ConfigAdapter] set_bool() called with an empty key");
        return;
    }

    ensure_node(root_, key) = value;
}

bool JSONConfigAdapter::has(const std::string& key) {
    return find_node(root_, key) != nullptr;
}

} // namespace Prism::Adapter
