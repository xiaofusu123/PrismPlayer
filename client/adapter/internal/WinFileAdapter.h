#pragma once

#include "FileAdapter.h"

namespace Prism::Adapter {

/**
 * @class WinFileAdapter
 * @brief Windows 平台文件 I/O 与元数据提取实现
 */
class WinFileAdapter : public FileAdapter {
public:
    WinFileAdapter() = default;
    ~WinFileAdapter() override = default;

    bool open(const std::string& file_path) override;
    void close() override;
    size_t read(uint8_t* buffer, size_t size) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    bool seek(int64_t offset, int whence) override;
    uint64_t get_size() override;
    FileMetadata get_metadata() override;
    bool is_open() override;

private:
    // TODO: 添加 Windows 文件句柄等私有成员
};

} // namespace Prism::Adapter
