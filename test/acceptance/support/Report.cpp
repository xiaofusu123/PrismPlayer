#include "Report.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

namespace Prism::Qa {

namespace {

std::string read_env(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return (value == nullptr || *value == '\0') ? fallback : std::string(value);
}

std::string trim(std::string text) {
    auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
    return text;
}

std::string current_utc() {
    std::time_t now = std::time(nullptr);
    const std::tm* utc = std::gmtime(&now);
    if (utc == nullptr) {
        return "unknown";
    }
    std::tm copy = *utc;
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &copy);
    return buffer;
}

std::string json_escape(const std::string& input) {
    std::ostringstream out;
    for (char ch : input) {
        switch (ch) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                char buffer[8]{};
                std::snprintf(buffer, sizeof(buffer), "\\u%04x",
                              static_cast<unsigned int>(static_cast<unsigned char>(ch)));
                out << buffer;
            } else {
                out << ch;
            }
            break;
        }
    }
    return out.str();
}

std::string quote_posix(const std::string& value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

std::string quote_powershell(const std::string& value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

std::string nullable_string(const std::string& value) {
    return value.empty() ? "null" : "\"" + json_escape(value) + "\"";
}

bool any_failed(const std::vector<CaseResult>& cases) {
    return std::any_of(cases.begin(), cases.end(),
                       [](const CaseResult& item) { return item.status == "failed"; });
}

} // namespace

std::string extract_skip_reason(const std::string& summary) {
    // gtest 的 skip 摘要就是 `GTEST_SKIP() <<` 写入的文本（可能带尾部空行）；
    // 某些版本会再带一个 "Skipped" 前缀，一并剥掉。
    std::string reason = trim(summary);
    std::string prefix = "Skipped";
    if (reason.rfind(prefix, 0) == 0) {
        reason = trim(reason.substr(prefix.size()));
    }
    return reason;
}

ReportContext load_report_context(int argc, char** argv) {
    ReportContext context;
    context.binary = (argc > 0 && argv[0] != nullptr) ? argv[0] : "test-acceptance";
    context.path = read_env("PRISM_E2E_REPORT", "");
    context.preset = read_env("PRISM_E2E_PRESET", "unknown");
    context.mode = read_env("PRISM_E2E_MODE", MODE_IN_PROCESS);
    context.commit = read_env("PRISM_E2E_COMMIT", "unknown");
    context.log_path = read_env("PRISM_E2E_LOG", "");
    context.perf_requested = read_env("PRISM_E2E_PERF", "0") == "1";
    context.started_utc = current_utc();
    return context;
}

CaseResult make_skipped_case(std::string name, std::string reason) {
    CaseResult result;
    result.name = std::move(name);
    result.status = "skipped";
    result.skip_reason = std::move(reason);
    result.duration_ms = 0;
    return result;
}

std::string make_repro(const ReportContext& context, const std::string& test_name) {
    // 复现命令必须能独立重跑：带上 preset / mode（报告里的失败发生在同一环境下），
    // 再用 --gtest_filter 只跑这条用例。
#ifdef _WIN32
    return "$env:PRISM_E2E_PRESET=" + quote_powershell(context.preset) +
           "; $env:PRISM_E2E_MODE=" + quote_powershell(context.mode) + "; & " +
           quote_powershell(context.binary) + " --gtest_filter=" + test_name;
#else
    return "PRISM_E2E_PRESET=" + quote_posix(context.preset) +
           " PRISM_E2E_MODE=" + quote_posix(context.mode) + " " + quote_posix(context.binary) +
           " --gtest_filter=" + test_name;
#endif
}

bool verdict_passed(const std::vector<CaseResult>& cases) {
    return !cases.empty() && !any_failed(cases);
}

bool write_report(const ReportContext& context, const std::vector<CaseResult>& cases,
                  std::chrono::milliseconds run_duration) {
    std::ofstream out(context.path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }

    out << "{\n";
    out << "  \"run\": {\n";
    out << "    \"commit\": \"" << json_escape(context.commit) << "\",\n";
    out << "    \"preset\": \"" << json_escape(context.preset) << "\",\n";
    out << "    \"mode\": \"" << json_escape(context.mode) << "\",\n";
    out << "    \"started_utc\": \"" << json_escape(context.started_utc) << "\",\n";
    out << "    \"duration_ms\": " << run_duration.count() << ",\n";
    out << "    \"media\": []\n";
    out << "  },\n";

    out << "  \"cases\": [";
    for (std::size_t index = 0; index < cases.size(); ++index) {
        const CaseResult& item = cases[index];
        out << (index == 0 ? "\n" : ",\n");
        out << "    {\n";
        out << "      \"name\": \"" << json_escape(item.name) << "\",\n";
        out << "      \"status\": \"" << json_escape(item.status) << "\",\n";
        out << "      \"skip_reason\": " << nullable_string(item.skip_reason) << ",\n";
        out << "      \"duration_ms\": " << item.duration_ms << ",\n";
        out << "      \"metrics\": {\n";
        out << "        \"first_frame_ms\": null,\n";
        out << "        \"seek_ms\": null,\n";
        out << "        \"presented\": 0,\n";
        out << "        \"dropped\": 0,\n";
        out << "        \"waited\": 0,\n";
        out << "        \"cpu_user_s\": null,\n";
        out << "        \"peak_rss_mib\": null,\n";
        out << "        \"inter_client_skew_ms\": null\n";
        out << "      },\n";
        out << "      \"evidence\": {\n";
        out << "        \"log\": " << nullable_string(context.log_path) << ",\n";
        out << "        \"repro\": "
            << nullable_string(item.status == "failed" ? make_repro(context, item.name) : "")
            << "\n";
        out << "      }\n";
        out << "    }";
    }
    out << (cases.empty() ? "],\n" : "\n  ],\n");

    out << "  \"coverage\": { \"av_sync\": null, \"service\": null, \"threadpool\": null },\n";
    out << "  \"verdict\": \"" << (verdict_passed(cases) ? "passed" : "failed") << "\"\n";
    out << "}\n";

    return static_cast<bool>(out);
}

void print_summary(const ReportContext& context, const std::vector<CaseResult>& cases) {
    std::size_t passed = 0;
    std::size_t skipped = 0;
    std::size_t failed = 0;
    for (const CaseResult& item : cases) {
        if (item.status == "passed") {
            ++passed;
        } else if (item.status == "skipped") {
            ++skipped;
        } else if (item.status == "failed") {
            ++failed;
        }
    }

    std::cout << "\n[acceptance] 报告：" << (context.path.empty() ? "（未生成）" : context.path)
              << "\n";
    std::cout << "[acceptance] run：mode=" << context.mode << " preset=" << context.preset
              << " commit=" << context.commit << "\n";
    std::cout << "[acceptance] 用例：跑了 " << (passed + failed) << " 条（通过 " << passed
              << " / 失败 " << failed << "），跳过 " << skipped << " 条，共 " << cases.size()
              << " 条\n";
    std::cout << "[acceptance] verdict：" << (verdict_passed(cases) ? "passed" : "failed") << "\n";
}

/* ========== ReportListener ========== */

void ReportListener::OnTestStart(const ::testing::TestInfo& test_info) {
    current_name_ = std::string(test_info.test_suite_name()) + "." + test_info.name();
    current_skip_reason_.clear();
    current_started_ = std::chrono::steady_clock::now();
}

void ReportListener::OnTestPartResult(const ::testing::TestPartResult& part_result) {
    if (part_result.skipped()) {
        current_skip_reason_ = extract_skip_reason(part_result.summary());
    }
}

void ReportListener::OnTestEnd(const ::testing::TestInfo& test_info) {
    CaseResult result;
    result.name = current_name_;
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - current_started_)
                             .count();

    if (test_info.result()->Skipped()) {
        result.status = "skipped";
        result.skip_reason =
            current_skip_reason_.empty() ? "用例跳过但未提供原因" : current_skip_reason_;
    } else if (test_info.result()->Failed()) {
        result.status = "failed";
    } else {
        result.status = "passed";
    }

    cases_.push_back(std::move(result));
}

} // namespace Prism::Qa
