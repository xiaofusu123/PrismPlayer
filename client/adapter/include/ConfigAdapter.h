#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace Prism::Adapter {

/**
 * @class ConfigAdapter
 * @brief JSON 配置文件读写抽象接口
 *
 * @note 键名支持 `.` 分隔的层级路径（如 `audio.sample_rate`）。空串、含空段的键
 *       （`a..b` / `.a` / `a.`）一律非法：读取返回 nullopt、has() 返回 false、
 *       set_*() 忽略并记日志。
 * @note 类型化读取按目标类型判定，类型不符返回 nullopt；唯一例外是 get_double()，
 *       它接受 JSON 里写成整数的数值。
 * @note **不抛异常**：缺文件、坏 JSON、根不是 object、类型不符、序列化失败、
 *       磁盘/权限错误都以 `false` 或 `nullopt` 表达。
 */
class ConfigAdapter {
  public:
    virtual ~ConfigAdapter() = default;

    /**
     * @brief 加载配置文件到内存文档，不改动磁盘
     * @param file_path 配置文件路径
     * @return 成功返回 true；缺文件、坏 JSON、根不是 object 一律返回 false
     * @note 失败不改动当前内存文档与当前目标；成功才把 file_path 设为当前目标。
     */
    virtual bool load(const std::string& file_path) = 0;

    /**
     * @brief 把内存文档写到当前目标文件（原子替换）
     * @return 成功返回 true；尚未 load()/save_as() 成功过返回 false
     */
    virtual bool save() = 0;

    /**
     * @brief 把内存文档写到指定路径，成功后把该路径设为新的当前目标
     * @param file_path 目标文件路径
     * @return 成功返回 true；失败返回 false 且**不改动当前目标**
     * @note 语义：save_as(B) 成功后，后续 save() 写的是 B，不是原来的 A。
     */
    virtual bool save_as(const std::string& file_path) = 0;

    // ==================== 类型化读取 ====================

    /**
     * @brief 读取字符串配置
     * @param key 配置键名
     * @return 找到返回值，否则返回 nullopt
     */
    virtual std::optional<std::string> get_string(const std::string& key) = 0;

    /**
     * @brief 读取整数配置
     * @param key 配置键名
     * @return 找到返回值，否则返回 nullopt
     */
    virtual std::optional<int64_t> get_int(const std::string& key) = 0;

    /**
     * @brief 读取浮点配置
     * @param key 配置键名
     * @return 找到返回值，否则返回 nullopt
     */
    virtual std::optional<double> get_double(const std::string& key) = 0;

    /**
     * @brief 读取布尔配置
     * @param key 配置键名
     * @return 找到返回值，否则返回 nullopt
     */
    virtual std::optional<bool> get_bool(const std::string& key) = 0;

    // ==================== 类型化写入 ====================

    /**
     * @brief 设置字符串配置
     * @param key 配置键名
     * @param value 配置值
     */
    virtual void set_string(const std::string& key, const std::string& value) = 0;

    /**
     * @brief 设置整数配置
     * @param key 配置键名
     * @param value 配置值
     */
    virtual void set_int(const std::string& key, int64_t value) = 0;

    /**
     * @brief 设置浮点配置
     * @param key 配置键名
     * @param value 配置值
     */
    virtual void set_double(const std::string& key, double value) = 0;

    /**
     * @brief 设置布尔配置
     * @param key 配置键名
     * @param value 配置值
     */
    virtual void set_bool(const std::string& key, bool value) = 0;

    // ==================== 工具方法 ====================

    /**
     * @brief 检查配置键是否存在
     * @param key 配置键名
     * @return 路径上每一级都存在即返回 true（含中间对象，如 `audio.sample_rate`
     *         存在时 `has("audio")` 也为 true）；键非法或任一级缺失返回 false
     * @note 只看「有没有这个节点」，不看值的类型：值为 null 也算存在。
     */
    virtual bool has(const std::string& key) = 0;
};

} // namespace Prism::Adapter
