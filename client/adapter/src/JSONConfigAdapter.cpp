#include "JSONConfigAdapter.h"

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

namespace Prism::Adapter {
namespace {

/** @brief dump() 的缩进空格数 */
constexpr int DUMP_INDENT = 4;

/** @brief 临时文件名前缀；临时文件与目标同目录，rename 才是同一文件系统内的原子替换 */
constexpr const char* TEMP_PREFIX = ".tmp-";

/**
 * @brief 按 `.` 切分配置键，得到层级路径
 * @param key 配置键名，如 `audio.sample_rate`
 * @return 各级键名
 */
[[nodiscard]] std::vector<std::string> split_key(const std::string& key) {
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
 * @brief 键是否合法：非空，且每一段都非空
 *
 * `""`、`"a..b"`、`".a"`、`"a."` 都非法。读取与写入共用这一条判定，否则会出现
 * 「set 得进去、has 却永远查不到」的不一致。
 */
[[nodiscard]] bool is_valid_key(const std::string& key) {
    if (key.empty()) {
        return false;
    }
    for (const auto& part : split_key(key)) {
        if (part.empty()) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 在配置树里按键查找节点
 * @param root 配置树根节点
 * @param key 配置键名
 * @return 命中返回节点指针，未命中或 key 非法返回 nullptr
 */
[[nodiscard]] const nlohmann::json* find_node(const nlohmann::json& root, const std::string& key) {
    if (!is_valid_key(key)) {
        return nullptr;
    }

    const nlohmann::json* node = &root;
    for (const auto& part : split_key(key)) {
        if (!node->is_object()) {
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
 * @param key 配置键名，调用方保证 is_valid_key() 为 true
 * @return 可写的节点引用
 */
[[nodiscard]] nlohmann::json& ensure_node(nlohmann::json& root, const std::string& key) {
    nlohmann::json* node = &root;
    for (const auto& part : split_key(key)) {
        if (!node->is_object()) {
            *node = nlohmann::json::object();
        }
        node = &(*node)[part];
    }
    return *node;
}

/**
 * @brief 生成与目标同目录、命名唯一的临时文件路径
 * @param target 目标文件路径
 * @return 形如 `<target>.tmp-<时间>-<随机数>` 的路径
 *
 * 时间戳 + `random_device` 组合，避免同一进程连续两次 save、或两个进程同时写
 * 同一份配置时撞名。不使用任何文件级静态变量（spec #117 决策 3）。
 */
[[nodiscard]] std::filesystem::path make_temp_path(const std::filesystem::path& target) {
    std::random_device rd;
    std::uint64_t nonce = (static_cast<std::uint64_t>(rd()) << 32) | rd();
    std::uint64_t stamp =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());

    std::filesystem::path temp = target;
    temp += TEMP_PREFIX;
    temp += std::to_string(stamp);
    temp += "-";
    temp += std::to_string(nonce);
    return temp;
}

/** @brief 删除临时文件；失败只记日志，不掩盖原始错误 */
void remove_temp_file(const std::filesystem::path& temp) {
    std::error_code ec;
    std::filesystem::remove(temp, ec);
    if (ec) {
        spdlog::warn("[ConfigAdapter] cannot remove temporary file '{}': {}", temp.string(),
                     ec.message());
    }
}

/**
 * @brief 把配置树原子地写到目标文件
 * @param target 目标文件路径，父目录不存在时自动创建
 * @param root 配置树
 * @return 成功返回 true
 *
 * 顺序：`dump()` 先在内存里完成（失败不碰磁盘）→ 写同目录临时文件 →
 * `flush()` + `close()` 校验 → `rename()` 原子替换。**目标文件在整个过程中从未被
 * 打开**，所以任何时刻被中断，目标只会是完整的旧内容或完整的新内容；最坏情况是
 * 留一个临时文件，而它永远不会被 `load()` 读到。
 *
 * Windows 上 `rename` 覆盖已存在文件由标准库实现保证（实测 clang64 / mingw64
 * 两套工具链均成功）；若目标被其他进程占用导致替换失败，本函数返回 false 并清理
 * 临时文件，目标保持旧内容 —— 不退化成「先删目标再改名」，那会破坏原子性。
 */
[[nodiscard]] bool write_json_atomic(const std::filesystem::path& target,
                                     const nlohmann::json& root) {
    std::string text;
    try {
        text = root.dump(DUMP_INDENT) + '\n';
    } catch (const std::exception& e) {
        spdlog::error("[ConfigAdapter] cannot serialize config for '{}': {}", target.string(),
                      e.what());
        return false;
    }

    std::error_code ec;
    std::filesystem::path parent = target.parent_path();
    if (!parent.empty()) {
        bool parent_exists = std::filesystem::exists(parent, ec);
        if (ec) {
            spdlog::error("[ConfigAdapter] cannot stat directory '{}': {}", parent.string(),
                          ec.message());
            return false;
        }
        if (!parent_exists) {
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                spdlog::error("[ConfigAdapter] cannot create directory '{}': {}", parent.string(),
                              ec.message());
                return false;
            }
        }
    }

    // temp 一旦出现在磁盘上，后续任何失败（含日志/路径转换抛出的异常）都必须删掉它。
    // 包一层 try/catch 而不是 RAII 类：作用域小、只有一个清理点，KISS。
    std::filesystem::path temp = make_temp_path(target);
    try {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            spdlog::error("[ConfigAdapter] cannot open temporary file '{}'", temp.string());
            remove_temp_file(temp);
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out) {
            spdlog::error("[ConfigAdapter] failed to write temporary file '{}'", temp.string());
            out.close();
            remove_temp_file(temp);
            return false;
        }
        // close() 会把「缓冲区刷到磁盘」这一步的错误暴露到流状态上，不能只查 flush()。
        out.close();
        if (!out) {
            spdlog::error("[ConfigAdapter] failed to close temporary file '{}'", temp.string());
            remove_temp_file(temp);
            return false;
        }

        std::filesystem::rename(temp, target, ec);
        if (ec) {
            spdlog::error("[ConfigAdapter] cannot replace '{}': {}", target.string(), ec.message());
            remove_temp_file(temp);
            return false;
        }

        spdlog::debug("[ConfigAdapter] wrote '{}' atomically", target.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[ConfigAdapter] atomic write to '{}' failed: {}", target.string(), e.what());
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return false;
    }
}

/**
 * @brief 类型化读取：类型不符返回 nullopt，读取时的异常不再向外传播
 * @tparam T 目标类型
 * @tparam Matcher 类型判定谓词
 * @param root 配置树根节点
 * @param key 配置键名
 * @param matches 判定节点是否为目标类型
 * @param type_name 日志里的类型描述
 * @return 命中且类型匹配返回值，否则返回 nullopt
 */
template <typename T, typename Matcher>
[[nodiscard]] std::optional<T> read_value(const nlohmann::json& root, const std::string& key,
                                          Matcher matches, const char* type_name) {
    const nlohmann::json* node = find_node(root, key);
    if (node == nullptr || !matches(*node)) {
        spdlog::debug("[ConfigAdapter] key '{}' is not {}", key, type_name);
        return std::nullopt;
    }
    try {
        return node->get<T>();
    } catch (const std::exception& e) {
        spdlog::error("[ConfigAdapter] cannot read key '{}' as {}: {}", key, type_name, e.what());
        return std::nullopt;
    }
}

/**
 * @brief 类型化写入：非法键静默忽略，写入时的异常不再向外传播
 * @tparam T 值类型
 * @param root 配置树根节点
 * @param key 配置键名
 * @param value 配置值
 * @param op 日志里的方法名
 */
template <typename T>
void write_value(nlohmann::json& root, const std::string& key, T&& value, const char* op) {
    if (!is_valid_key(key)) {
        spdlog::warn("[ConfigAdapter] {}() ignored invalid key '{}'", op, key);
        return;
    }
    try {
        ensure_node(root, key) = std::forward<T>(value);
    } catch (const std::exception& e) {
        spdlog::error("[ConfigAdapter] {}() failed for key '{}': {}", op, key, e.what());
    }
}

} // namespace

bool JSONConfigAdapter::load(const std::string& file_path) {
    try {
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

        nlohmann::json parsed = nlohmann::json::parse(in);
        if (!parsed.is_object()) {
            spdlog::error("[ConfigAdapter] root of '{}' must be a JSON object", file_path);
            return false;
        }

        root_ = std::move(parsed);
        file_path_ = file_path;
        spdlog::debug("[ConfigAdapter] loaded '{}'", file_path_);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[ConfigAdapter] load('{}') failed: {}", file_path, e.what());
        return false;
    }
}

bool JSONConfigAdapter::save() {
    if (file_path_.empty()) {
        spdlog::error("[ConfigAdapter] save() called before load() or save_as()");
        return false;
    }
    try {
        return write_json_atomic(std::filesystem::path(file_path_), root_);
    } catch (const std::exception& e) {
        spdlog::error("[ConfigAdapter] save() failed: {}", e.what());
        return false;
    }
}

bool JSONConfigAdapter::save_as(const std::string& file_path) {
    if (file_path.empty()) {
        spdlog::error("[ConfigAdapter] save_as() called with an empty path");
        return false;
    }
    try {
        if (!write_json_atomic(std::filesystem::path(file_path), root_)) {
            return false;
        }
        // 只有写成功了才切换当前目标：失败时 save() 仍指向原来那份文件。
        file_path_ = file_path;
        spdlog::debug("[ConfigAdapter] saved as '{}'", file_path_);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[ConfigAdapter] save_as('{}') failed: {}", file_path, e.what());
        return false;
    }
}

std::optional<std::string> JSONConfigAdapter::get_string(const std::string& key) {
    return read_value<std::string>(
        root_, key, [](const nlohmann::json& node) { return node.is_string(); }, "a string");
}

std::optional<int64_t> JSONConfigAdapter::get_int(const std::string& key) {
    return read_value<int64_t>(
        root_, key, [](const nlohmann::json& node) { return node.is_number_integer(); },
        "an integer");
}

std::optional<double> JSONConfigAdapter::get_double(const std::string& key) {
    return read_value<double>(
        root_, key, [](const nlohmann::json& node) { return node.is_number(); }, "a number");
}

std::optional<bool> JSONConfigAdapter::get_bool(const std::string& key) {
    return read_value<bool>(
        root_, key, [](const nlohmann::json& node) { return node.is_boolean(); }, "a boolean");
}

void JSONConfigAdapter::set_string(const std::string& key, const std::string& value) {
    write_value(root_, key, value, "set_string");
}

void JSONConfigAdapter::set_int(const std::string& key, int64_t value) {
    write_value(root_, key, value, "set_int");
}

void JSONConfigAdapter::set_double(const std::string& key, double value) {
    write_value(root_, key, value, "set_double");
}

void JSONConfigAdapter::set_bool(const std::string& key, bool value) {
    write_value(root_, key, value, "set_bool");
}

bool JSONConfigAdapter::has(const std::string& key) {
    return find_node(root_, key) != nullptr;
}

} // namespace Prism::Adapter
