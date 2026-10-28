#include "WinFileAdapter.h"

#include <cctype>
#include <cstdio>
#include <system_error>

#include <spdlog/spdlog.h>

namespace Prism::Adapter {
namespace {

/**
 * @brief 由扩展名推断容器格式
 * @param path 规范化后的文件路径
 * @return 不含点的大写格式串（如 `MP4` / `FLAC`）；无扩展名时返回空串
 */
std::string detect_format(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    if (ext.empty()) {
        return {};
    }

    if (ext.front() == '.') {
        ext.erase(0, 1);
    }

    for (char& ch : ext) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return ext;
}

/**
 * @brief 把 whence 的整数取值映射到 std::ios_base::seekdir
 * @param whence SEEK_SET / SEEK_CUR / SEEK_END
 * @param dir 映射结果，仅在本函数返回 true 时有效
 * @return 无法识别的 whence 返回 false
 */
bool to_seekdir(int whence, std::ios_base::seekdir& dir) {
    switch (whence) {
    case SEEK_SET:
        dir = std::ios_base::beg;
        return true;
    case SEEK_CUR:
        dir = std::ios_base::cur;
        return true;
    case SEEK_END:
        dir = std::ios_base::end;
        return true;
    default:
        return false;
    }
}

} // namespace

WinFileAdapter::~WinFileAdapter() {
    close();
}

bool WinFileAdapter::open(const std::string& file_path) {
    close();

    if (file_path.empty()) {
        spdlog::error("[FileAdapter] open() called with an empty path");
        return false;
    }

    std::error_code ec;
    // 不加 const：.clang-tidy 的 ConstantCase=UPPER_CASE 会连函数内的 const 局部变量一起管，
    // 而 §1.2 要求变量是 snake_case，两者对局部变量是冲突的。
    std::filesystem::path normalized =
        std::filesystem::weakly_canonical(std::filesystem::path(file_path), ec);
    if (ec) {
        spdlog::error("[FileAdapter] cannot normalize path '{}': {}", file_path, ec.message());
        return false;
    }

    bool exists = std::filesystem::exists(normalized, ec);
    if (ec) {
        spdlog::error("[FileAdapter] cannot stat '{}': {}", normalized.string(), ec.message());
        return false;
    }

    if (exists && std::filesystem::is_directory(normalized)) {
        spdlog::error("[FileAdapter] '{}' is a directory, not a media file", normalized.string());
        return false;
    }

    auto mode = std::ios::in | std::ios::out | std::ios::binary;
    if (!exists) {
        // 只在这里补 trunc：新建文件才需要截断，已有文件必须原样打开。
        mode |= std::ios::trunc;
        spdlog::info("[FileAdapter] '{}' does not exist, creating an empty file",
                     normalized.string());
    }

    stream_.open(normalized, mode);
    if (!stream_.is_open()) {
        spdlog::error("[FileAdapter] failed to open '{}'", normalized.string());
        return false;
    }

    path_ = normalized;
    spdlog::debug("[FileAdapter] opened '{}' ({} bytes)", path_.string(), get_size());
    return true;
}

void WinFileAdapter::close() {
    if (stream_.is_open()) {
        stream_.close();
        spdlog::debug("[FileAdapter] closed '{}'", path_.string());
    }

    stream_.clear();
    path_.clear();
}

size_t WinFileAdapter::read(uint8_t* buffer, size_t size) {
    if (buffer == nullptr) {
        spdlog::error("[FileAdapter] read() called with a null buffer");
        return 0;
    }

    if (size == 0) {
        return 0;
    }

    if (!stream_.is_open()) {
        spdlog::warn("[FileAdapter] read() called before open()");
        return 0;
    }

    stream_.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(size));
    std::streamsize read_bytes = stream_.gcount();
    if (read_bytes <= 0) {
        spdlog::debug("[FileAdapter] read() reached EOF on '{}'", path_.string());
        return 0;
    }

    return static_cast<size_t>(read_bytes);
}

size_t WinFileAdapter::write(const uint8_t* buffer, size_t size) {
    if (buffer == nullptr) {
        spdlog::error("[FileAdapter] write() called with a null buffer");
        return 0;
    }

    if (size == 0) {
        return 0;
    }

    if (!stream_.is_open()) {
        spdlog::warn("[FileAdapter] write() called before open()");
        return 0;
    }

    stream_.write(reinterpret_cast<const char*>(buffer), static_cast<std::streamsize>(size));
    if (!stream_) {
        spdlog::error("[FileAdapter] write() failed on '{}'", path_.string());
        return 0;
    }

    return size;
}

bool WinFileAdapter::seek(int64_t offset, int whence) {
    if (!stream_.is_open()) {
        spdlog::warn("[FileAdapter] seek() called before open()");
        return false;
    }

    std::ios_base::seekdir dir = std::ios_base::beg;
    if (!to_seekdir(whence, dir)) {
        spdlog::error("[FileAdapter] unknown whence: {}", whence);
        return false;
    }

    // 读到 EOF 会置 failbit，不先清掉的话后续 seek 一律失败。
    stream_.clear();
    stream_.seekg(static_cast<std::streamoff>(offset), dir);
    stream_.seekp(static_cast<std::streamoff>(offset), dir);
    if (!stream_) {
        spdlog::error("[FileAdapter] seek({}, {}) failed on '{}'", offset, whence, path_.string());
        return false;
    }

    return true;
}

uint64_t WinFileAdapter::get_size() {
    if (path_.empty()) {
        spdlog::warn("[FileAdapter] get_size() called before open()");
        return 0;
    }

    // 写进去的数据还压在流缓冲区里时，file_size 看到的是落盘前的旧大小，先 flush 交出去。
    if (stream_.is_open()) {
        stream_.flush();
    }

    std::error_code ec;
    uintmax_t size = std::filesystem::file_size(path_, ec);
    if (ec) {
        spdlog::error("[FileAdapter] file_size() failed on '{}': {}", path_.string(), ec.message());
        return 0;
    }

    return static_cast<uint64_t>(size);
}

FileMetadata WinFileAdapter::get_metadata() {
    FileMetadata metadata;
    metadata.file_path = path_.empty() ? std::string() : path_.string();
    metadata.file_size = get_size();
    metadata.format = detect_format(path_);

    // 其余字段（时长 / 码率 / 采样率 / 分辨率 / 编码器等）必须解封装容器才能拿到，
    // 按 FileAdapter.h「非适用字段为零值」的契约保持零值，等 Engine 层 Demuxer 接入后补齐。
    return metadata;
}

bool WinFileAdapter::is_open() {
    return stream_.is_open();
}

} // namespace Prism::Adapter
