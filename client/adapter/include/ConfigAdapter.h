#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace Prism::Adapter {

/**
 * @class ConfigAdapter
 * @brief JSON 配置文件读写抽象接口
 */
class ConfigAdapter {
public:
    virtual ~ConfigAdapter() = default;

    /**
     * @brief 加载配置文件
     * @param file_path 配置文件路径
     * @return 成功返回 true
     */
    virtual bool load(const std::string& file_path) = 0;

    /**
     * @brief 保存配置到当前文件
     * @return 成功返回 true
     */
    virtual bool save() = 0;

    /**
     * @brief 另存为指定路径
     * @param file_path 目标文件路径
     * @return 成功返回 true
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
     * @return 存在返回 true
     */
    virtual bool has(const std::string& key) = 0;
};

} // namespace Prism::Adapter
