#pragma once

#include "FileAdapter.h"

#include <filesystem>
#include <fstream>

namespace Prism::Adapter {

/**
 * @class WinFileAdapter
 * @brief 文件 I/O 与元数据提取实现
 *
 * 基于 std::filesystem + std::fstream，不引入新的第三方依赖：
 * - open() 先做路径规范化（std::filesystem::weakly_canonical，去掉 `.` / `..`
 *   与多余分隔符），再以二进制读写方式打开；文件不存在时创建空文件并记 info 日志。
 * - get_size() 走 std::filesystem::file_size，不依赖流状态，因此读失败后仍可查大小。
 * - get_metadata() 目前只能填出 file_path / file_size / format（由扩展名推断、转大写）；
 *   时长、码率、分辨率、编码器等需要 FFmpeg 解封装才能拿到，按 FileAdapter.h
 *   的「非适用字段为零值」契约留零值，等 Engine 层的 Demuxer 接入后补。
 *
 * 类名沿用历史命名（WinFileAdapter），但实现本身是跨平台的；
 * 与 CryptoAdapter / DBAdapter 那组命名对齐一起，由命名对齐工单（G3）决定。
 */
class WinFileAdapter : public FileAdapter {
  public:
    WinFileAdapter() = default;
    ~WinFileAdapter() override;

    bool open(const std::string& file_path) override;
    void close() override;
    size_t read(uint8_t* buffer, size_t size) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    bool seek(int64_t offset, int whence) override;
    uint64_t get_size() override;
    FileMetadata get_metadata() override;
    bool is_open() override;

  private:
    std::fstream stream_;        /**< 文件流（二进制、可读可写） */
    std::filesystem::path path_; /**< 规范化后的文件路径，未打开时为空 */
};

} // namespace Prism::Adapter
