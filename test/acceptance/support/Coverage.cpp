#include "support/Coverage.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace Prism::Qa {
namespace {

constexpr const char* WAIT_FOR_84 = "待 #84";

/**
 * @struct BatchMatcher
 * @brief 一个批次在 `llvm-cov report` 里的识别规则。
 */
struct BatchMatcher {
    const char* name{nullptr};          /**< 批次名，与 #87 报告契约同名 */
    const char* path_fragment{nullptr}; /**< 文件名里出现该片段就计入本批次 */
    const char* missing_hint{
        nullptr}; /**< 「测不到」的专用解释；nullptr 表示报告里没有该批次源文件 */
};

constexpr BatchMatcher BATCH_MATCHERS[COVERAGE_BATCH_COUNT] = {
    {"av_sync", "business/av_sync/src/", nullptr},
    {"service", "service/src/", nullptr},
    {"threadpool", "threadpool/",
     "无法测量：覆盖率口径排除 vendor/，且 vendor/threadpool 不参与构建（需 #84 调整口径）"},
};

/// `llvm-cov report` 每行 = 文件名 + 12 个数字列。
constexpr std::size_t LLVM_COV_COLUMN_COUNT = 12;
/// 总行数（Lines）在末尾 12 列中的位置。
constexpr std::size_t LINES_COLUMN_FROM_END = 6;
/// 未覆盖行数（Missed Lines）在末尾 12 列中的位置。
constexpr std::size_t MISSED_LINES_COLUMN_FROM_END = 5;

std::string normalize_path(std::string path) {
    for (char& ch : path) {
        if (ch == '\\') {
            ch = '/';
        }
    }
    return path;
}

std::vector<std::string> split_whitespace(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token) {
        tokens.push_back(std::move(token));
    }
    return tokens;
}

bool parse_integer(std::string_view token, long long& value) {
    const char* begin = token.data();
    const char* end = begin + token.size();
    std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc() && result.ptr == end;
}

bool starts_with(const std::string& text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

/**
 * @struct ReportRow
 * @brief 报告里一个源文件的行覆盖数字。
 */
struct ReportRow {
    std::string path;          /**< 源文件路径（已统一成 `/`） */
    long long lines{0};        /**< 总行数 */
    long long missed_lines{0}; /**< 未覆盖行数 */
};

std::optional<ReportRow> parse_row(const std::string& line) {
    if (line.empty() || starts_with(line, "Filename") || starts_with(line, "TOTAL") ||
        starts_with(line, "----")) {
        return std::nullopt;
    }

    std::vector<std::string> tokens = split_whitespace(line);
    if (tokens.size() <= LLVM_COV_COLUMN_COUNT) {
        return std::nullopt;
    }

    std::size_t count = tokens.size();
    long long lines = 0;
    long long missed_lines = 0;
    if (!parse_integer(tokens[count - LINES_COLUMN_FROM_END], lines) ||
        !parse_integer(tokens[count - MISSED_LINES_COLUMN_FROM_END], missed_lines)) {
        return std::nullopt;
    }

    // 文件名本身可能含空格：把最后 12 个数字列之前的所有 token 拼回一个路径。
    std::string path;
    for (std::size_t i = 0; i + LLVM_COV_COLUMN_COUNT < count; ++i) {
        if (!path.empty()) {
            path.push_back(' ');
        }
        path += tokens[i];
    }
    return ReportRow{normalize_path(std::move(path)), lines, missed_lines};
}

CoverageSummary make_pending(const std::string& source, const std::string& reason) {
    CoverageSummary summary;
    summary.source = source;
    for (std::size_t i = 0; i < COVERAGE_BATCH_COUNT; ++i) {
        summary.batches[i].name = BATCH_MATCHERS[i].name;
        summary.batches[i].reason = reason;
    }
    return summary;
}

} // namespace

double BatchCoverage::line_percent() const {
    if (lines <= 0) {
        return 0.0;
    }
    return 100.0 * static_cast<double>(covered_lines()) / static_cast<double>(lines);
}

bool CoverageSummary::any_measured() const {
    return std::any_of(batches.begin(), batches.end(),
                       [](const BatchCoverage& item) { return item.measured; });
}

const BatchCoverage& CoverageSummary::batch(const std::string& name) const {
    static const BatchCoverage UNKNOWN_BATCH{"unknown", false, 0, 0,
                                             std::string(WAIT_FOR_84) + "：未登记该批次"};
    for (const BatchCoverage& item : batches) {
        if (item.name == name) {
            return item;
        }
    }
    return UNKNOWN_BATCH;
}

CoverageSummary parse_coverage_report(const std::string& text) {
    std::array<long long, COVERAGE_BATCH_COUNT> lines{};
    std::array<long long, COVERAGE_BATCH_COUNT> missed_lines{};

    std::istringstream stream(text);
    std::string line;
    std::size_t rows = 0;
    while (std::getline(stream, line)) {
        std::optional<ReportRow> row = parse_row(line);
        if (!row.has_value()) {
            continue;
        }
        ++rows;
        for (std::size_t i = 0; i < COVERAGE_BATCH_COUNT; ++i) {
            if (row->path.find(BATCH_MATCHERS[i].path_fragment) == std::string::npos) {
                continue;
            }
            lines[i] += row->lines;
            missed_lines[i] += row->missed_lines;
        }
    }

    if (rows == 0) {
        return make_pending({}, std::string(WAIT_FOR_84) + "：覆盖率报告为空或格式不可识别");
    }

    CoverageSummary summary;
    for (std::size_t i = 0; i < COVERAGE_BATCH_COUNT; ++i) {
        BatchCoverage& batch = summary.batches[i];
        batch.name = BATCH_MATCHERS[i].name;
        batch.lines = lines[i];
        batch.missed_lines = missed_lines[i];
        batch.measured = batch.lines > 0;
        if (batch.measured) {
            // 默认值给的是 pending 文案，在这里要抹掉，免得「已测得」还带待办原因。
            batch.reason.clear();
            continue;
        }
        if (BATCH_MATCHERS[i].missing_hint != nullptr) {
            batch.reason = BATCH_MATCHERS[i].missing_hint;
        } else {
            batch.reason = std::string(WAIT_FOR_84) + "：报告中未找到 " +
                           BATCH_MATCHERS[i].path_fragment + " 下的源文件";
        }
    }
    return summary;
}

CoverageSummary load_coverage_summary(const std::string& path) {
    if (path.empty()) {
        return make_pending({}, std::string(WAIT_FOR_84) + "：未设置 PRISM_E2E_COVERAGE_REPORT");
    }

    std::ifstream stream(path);
    if (!stream) {
        return make_pending(path, std::string(WAIT_FOR_84) + "：未找到覆盖率报告 " + path +
                                      "（先运行 scripts/build/coverage.ps1 或 coverage.sh）");
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    CoverageSummary summary = parse_coverage_report(buffer.str());
    summary.source = path;
    return summary;
}

} // namespace Prism::Qa
