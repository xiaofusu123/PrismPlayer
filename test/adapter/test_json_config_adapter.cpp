#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "JSONConfigAdapter.h"

namespace {

using Prism::Adapter::JSONConfigAdapter;

/** @brief 造一个进程内唯一的临时目录，用例之间互不污染 */
std::filesystem::path make_temp_dir() {
    std::random_device rd;
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("prism-config-tests-" + std::to_string(rd()) + "-" + std::to_string(rd()));
    std::filesystem::create_directories(dir);
    return dir;
}

/** @brief 往路径写一段文本，失败时给用例记一条失败而不是静默跳过 */
void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        ADD_FAILURE() << "cannot write " << path;
        return;
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

/** @brief 逐字节读回文件，用于断言「旧内容没有被截断」 */
std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        ADD_FAILURE() << "cannot read " << path;
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

/**
 * @brief 列出目录里所有临时文件（文件名含 `.tmp-`）
 * @return 文件名列表；正常/失败的写入路径都应该是空
 */
std::vector<std::string> list_temp_files(const std::filesystem::path& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.path().filename().string().find(".tmp-") != std::string::npos) {
            names.push_back(entry.path().filename().string());
        }
    }
    return names;
}

class JSONConfigAdapterTest : public ::testing::Test {
  protected:
    void SetUp() override { dir_ = make_temp_dir(); }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }

    [[nodiscard]] std::filesystem::path path(const std::string& name) const { return dir_ / name; }

    std::filesystem::path dir_;
};

TEST_F(JSONConfigAdapterTest, TypedRoundTripAllFourTypes) {
    JSONConfigAdapter adapter;

    adapter.set_string("general.nickname", "棱镜播放器");
    adapter.set_int("audio.sample_rate", 48000);
    adapter.set_int("audio.offset_ms", -250);
    adapter.set_double("video.gamma", 2.25);
    adapter.set_bool("general.fullscreen", false);
    adapter.set_bool("general.autoplay", true);
    adapter.set_string("general.empty", "");

    EXPECT_EQ(adapter.get_string("general.nickname"), std::optional<std::string>("棱镜播放器"));
    EXPECT_EQ(adapter.get_string("general.empty"), std::optional<std::string>(""));
    EXPECT_EQ(adapter.get_int("audio.sample_rate"), std::optional<std::int64_t>(48000));
    EXPECT_EQ(adapter.get_int("audio.offset_ms"), std::optional<std::int64_t>(-250));
    EXPECT_DOUBLE_EQ(*adapter.get_double("video.gamma"), 2.25);
    EXPECT_EQ(adapter.get_bool("general.fullscreen"), std::optional<bool>(false));
    EXPECT_EQ(adapter.get_bool("general.autoplay"), std::optional<bool>(true));
}

TEST_F(JSONConfigAdapterTest, DottedKeysCreateNestedObjects) {
    std::filesystem::path file = path("nested.json");
    JSONConfigAdapter adapter;

    adapter.set_string("ui.theme.primary", "#1e88e5");
    adapter.set_int("ui.theme.font_size", 14);
    adapter.set_int("ui.window.width", 1280);
    adapter.set_int("ui.window.height", 720);
    ASSERT_TRUE(adapter.save_as(file.string()));

    // 用 nlohmann::json 独立解析落盘文件：既证明真的消费了这个库，也证明嵌套形状。
    std::ifstream in(file, std::ios::binary);
    ASSERT_TRUE(in.is_open());
    nlohmann::json parsed = nlohmann::json::parse(in);
    EXPECT_EQ(parsed["ui"]["theme"]["primary"], "#1e88e5");
    EXPECT_EQ(parsed["ui"]["theme"]["font_size"], 14);
    EXPECT_EQ(parsed["ui"]["window"]["width"], 1280);
    EXPECT_EQ(parsed["ui"]["window"]["height"], 720);
    EXPECT_TRUE(parsed["ui"]["theme"].is_object());
    EXPECT_TRUE(parsed["ui"]["window"].is_object());

    // 重新 load 后仍能从内存文档读回同一批值。
    JSONConfigAdapter reloaded;
    ASSERT_TRUE(reloaded.load(file.string()));
    EXPECT_EQ(reloaded.get_string("ui.theme.primary"), std::optional<std::string>("#1e88e5"));
    EXPECT_EQ(reloaded.get_int("ui.window.width"), std::optional<std::int64_t>(1280));
}

TEST_F(JSONConfigAdapterTest, HasFollowsDocumentedSemantics) {
    std::filesystem::path file = path("has.json");
    write_text(file, R"({"audio":{"sample_rate":48000},"nullable":null})");

    JSONConfigAdapter adapter;
    ASSERT_TRUE(adapter.load(file.string()));

    EXPECT_TRUE(adapter.has("audio"));             // 中间对象也算存在
    EXPECT_TRUE(adapter.has("audio.sample_rate")); // 叶子节点
    EXPECT_TRUE(adapter.has("nullable"));          // 值为 null 也算存在
    EXPECT_FALSE(adapter.has("audio.device"));     // 任一级缺失
    EXPECT_FALSE(adapter.has("missing"));
    EXPECT_FALSE(adapter.has(""));            // 空键非法
    EXPECT_FALSE(adapter.has("audio..rate")); // 含空段非法
    EXPECT_FALSE(adapter.has(".audio"));
    EXPECT_FALSE(adapter.has("audio."));
}

TEST_F(JSONConfigAdapterTest, InvalidKeysAreIgnoredBySetters) {
    JSONConfigAdapter adapter;

    adapter.set_string("", "x");
    adapter.set_string("audio..rate", "x");
    adapter.set_int(".leading", 1);
    adapter.set_double("trailing.", 1.0);
    adapter.set_bool("a...b", true);

    EXPECT_FALSE(adapter.has(""));
    EXPECT_FALSE(adapter.has("audio..rate"));
    EXPECT_FALSE(adapter.has(".leading"));
    EXPECT_FALSE(adapter.has("trailing."));
    EXPECT_FALSE(adapter.has("a...b"));
    EXPECT_FALSE(adapter.get_string("audio..rate").has_value());
    EXPECT_FALSE(adapter.get_int(".leading").has_value());
}

TEST_F(JSONConfigAdapterTest, MissingFileReturnsFalseWithoutThrowing) {
    JSONConfigAdapter adapter;
    bool ok = true;
    EXPECT_NO_THROW(ok = adapter.load(path("no-such-file.json").string()));
    EXPECT_FALSE(ok);
    EXPECT_FALSE(adapter.save()); // 未成功 load 过，没有当前目标
}

TEST_F(JSONConfigAdapterTest, MalformedJsonReturnsFalseWithoutThrowing) {
    std::filesystem::path broken = path("broken.json");
    std::filesystem::path empty = path("empty.json");
    write_text(broken, R"({"audio": )");
    write_text(empty, "");

    JSONConfigAdapter adapter;
    bool ok = true;
    EXPECT_NO_THROW(ok = adapter.load(broken.string()));
    EXPECT_FALSE(ok);
    EXPECT_NO_THROW(ok = adapter.load(empty.string()));
    EXPECT_FALSE(ok);
}

TEST_F(JSONConfigAdapterTest, NonObjectRootReturnsFalseWithoutThrowing) {
    std::filesystem::path array_root = path("array.json");
    std::filesystem::path scalar_root = path("scalar.json");
    write_text(array_root, R"([1, 2, 3])");
    write_text(scalar_root, "42");

    JSONConfigAdapter adapter;
    bool ok = true;
    EXPECT_NO_THROW(ok = adapter.load(array_root.string()));
    EXPECT_FALSE(ok);
    EXPECT_NO_THROW(ok = adapter.load(scalar_root.string()));
    EXPECT_FALSE(ok);
}

TEST_F(JSONConfigAdapterTest, TypeMismatchReturnsNulloptWithoutThrowing) {
    std::filesystem::path file = path("types.json");
    write_text(file, R"({"s":"text","i":7,"d":1.5,"b":true,"n":null})");

    JSONConfigAdapter adapter;
    ASSERT_TRUE(adapter.load(file.string()));

    EXPECT_FALSE(adapter.get_string("i").has_value());
    EXPECT_FALSE(adapter.get_int("d").has_value()); // 浮点不是整数
    EXPECT_FALSE(adapter.get_int("b").has_value()); // bool 不是整数
    EXPECT_FALSE(adapter.get_bool("i").has_value());
    EXPECT_FALSE(adapter.get_double("s").has_value());
    EXPECT_FALSE(adapter.get_string("n").has_value());
    EXPECT_FALSE(adapter.get_string("absent").has_value());

    // 文档写明的唯一例外：JSON 里写成整数的数值可以被 get_double() 读出。
    EXPECT_DOUBLE_EQ(*adapter.get_double("i"), 7.0);

    std::optional<std::string> value;
    EXPECT_NO_THROW(value = adapter.get_string("i"));
    EXPECT_FALSE(value.has_value());
}

TEST_F(JSONConfigAdapterTest, LoadDoesNotWriteAndSaveDoes) {
    std::filesystem::path file = path("load-vs-save.json");
    std::string original = R"({"version":1})";
    write_text(file, original);

    JSONConfigAdapter adapter;
    ASSERT_TRUE(adapter.load(file.string()));
    adapter.set_int("version", 2);
    EXPECT_EQ(read_text(file), original) << "set_* / load 都不该改动磁盘";

    ASSERT_TRUE(adapter.save());
    std::string saved = read_text(file);
    EXPECT_NE(saved, original);
    EXPECT_EQ(nlohmann::json::parse(saved)["version"], 2);
}

TEST_F(JSONConfigAdapterTest, SaveReplacesTargetCompletelyAndConsumesTempFile) {
    std::filesystem::path file = path("atomic.json");
    std::string original = R"({"version":1,"group":{"keep":"yes"}})";
    write_text(file, original);

    JSONConfigAdapter adapter;
    ASSERT_TRUE(adapter.load(file.string()));
    adapter.set_int("version", 2);
    adapter.set_string("group.added", "new value");
    ASSERT_TRUE(adapter.save());

    // 目标是完整的新内容（可被独立解析），不是旧内容尾部追加了新内容。
    std::string saved = read_text(file);
    nlohmann::json parsed = nlohmann::json::parse(saved);
    EXPECT_EQ(parsed["version"], 2);
    EXPECT_EQ(parsed["group"]["keep"], "yes");
    EXPECT_EQ(parsed["group"]["added"], "new value");
    EXPECT_EQ(saved.find("original"), std::string::npos);

    // 临时文件被 rename 消化掉，目录里不留任何 .tmp- 文件。
    EXPECT_TRUE(list_temp_files(dir_).empty());
}

TEST_F(JSONConfigAdapterTest, SerializationFailureKeepsOldContentAndNoTempFile) {
    std::filesystem::path file = path("bad-utf8.json");
    std::string original = R"({"version":1,"group":{"keep":"yes"}})";
    write_text(file, original);

    JSONConfigAdapter adapter;
    ASSERT_TRUE(adapter.load(file.string()));
    // dump() 对非法 UTF-8 会抛 type_error.316：这是「写入过程中被中断」的注入点。
    adapter.set_string("group.bad", std::string(1, static_cast<char>(0xFF)));

    bool ok = true;
    EXPECT_NO_THROW(ok = adapter.save());
    EXPECT_FALSE(ok);
    EXPECT_EQ(read_text(file), original) << "序列化失败时目标必须逐字节保持旧内容";
    EXPECT_TRUE(list_temp_files(dir_).empty()) << "失败路径不能留下临时文件";
}

TEST_F(JSONConfigAdapterTest, ReplacementFailureKeepsTargetAndCleansTempFile) {
    std::filesystem::path file = path("replace.json");
    std::string original = R"({"version":1})";
    write_text(file, original);

    JSONConfigAdapter adapter;
    ASSERT_TRUE(adapter.load(file.string()));
    adapter.set_int("version", 2);

    // 让目标路径是一个已存在的非空目录：临时文件能写成功，但 rename 到目录必然
    // 失败（POSIX EISDIR / Windows 拒绝）。这就是「替换阶段被中断」的注入点。
    std::filesystem::path blocked = path("blocked-dir");
    std::filesystem::create_directories(blocked);
    std::string sentinel = R"({"untouched":true})";
    write_text(blocked / "old.json", sentinel);

    bool ok = true;
    EXPECT_NO_THROW(ok = adapter.save_as(blocked.string()));
    EXPECT_FALSE(ok);
    EXPECT_TRUE(std::filesystem::is_directory(blocked)) << "目标不能被临时文件替换掉";
    EXPECT_EQ(read_text(blocked / "old.json"), sentinel) << "目标里已有的内容必须完整保留";
    EXPECT_TRUE(list_temp_files(dir_).empty()) << "替换失败也要清掉临时文件";

    // save_as 失败不切换当前目标，后续 save() 仍写回原文件。
    EXPECT_EQ(read_text(file), original);
    ASSERT_TRUE(adapter.save());
    EXPECT_EQ(nlohmann::json::parse(read_text(file))["version"], 2);
}

TEST_F(JSONConfigAdapterTest, SaveAsRetargetsSubsequentSave) {
    std::filesystem::path first = path("first.json");
    std::filesystem::path second = path("second.json");
    write_text(first, R"({"version":1})");

    JSONConfigAdapter adapter;
    ASSERT_TRUE(adapter.load(first.string()));
    adapter.set_int("version", 2);
    ASSERT_TRUE(adapter.save_as(second.string()));

    EXPECT_EQ(nlohmann::json::parse(read_text(second))["version"], 2);
    EXPECT_EQ(read_text(first), R"({"version":1})") << "save_as 不应改写原文件";

    // 语义钉死：save_as(B) 成功后，后续 save() 写的是 B。
    adapter.set_int("version", 3);
    ASSERT_TRUE(adapter.save());
    EXPECT_EQ(nlohmann::json::parse(read_text(second))["version"], 3);
    EXPECT_EQ(read_text(first), R"({"version":1})") << "后续 save() 不能再写回 A";
    EXPECT_TRUE(list_temp_files(dir_).empty());
}

TEST_F(JSONConfigAdapterTest, SaveAsFailureKeepsOriginalTarget) {
    std::filesystem::path first = path("first.json");
    write_text(first, R"({"version":1})");

    JSONConfigAdapter adapter;
    ASSERT_TRUE(adapter.load(first.string()));
    adapter.set_int("version", 2);

    // 空路径是明确的失败输入；save_as 失败不改变当前目标。
    bool ok = true;
    EXPECT_NO_THROW(ok = adapter.save_as(""));
    EXPECT_FALSE(ok);
    ASSERT_TRUE(adapter.save());
    EXPECT_EQ(nlohmann::json::parse(read_text(first))["version"], 2);
}

TEST_F(JSONConfigAdapterTest, LoadFailureDoesNotChangeCurrentTarget) {
    std::filesystem::path good = path("good.json");
    std::filesystem::path broken = path("broken.json");
    write_text(good, R"({"version":1})");
    write_text(broken, R"({"version":)");

    JSONConfigAdapter adapter;
    ASSERT_TRUE(adapter.load(good.string()));
    ASSERT_FALSE(adapter.load(broken.string()));

    EXPECT_EQ(adapter.get_int("version"), std::optional<std::int64_t>(1))
        << "load 失败不应清空已加载的内存文档";
    adapter.set_int("version", 2);
    ASSERT_TRUE(adapter.save());
    EXPECT_EQ(nlohmann::json::parse(read_text(good))["version"], 2);
}

TEST_F(JSONConfigAdapterTest, ReadersNeverSeePartialJsonDuringReplacement) {
    std::filesystem::path file = path("concurrent.json");
    // 1 MiB 载荷把「写临时文件」这一步拉长成一个可持续采样的窗口，避免高速机器上
    // 观察者还没启动就写完了（这个用例不是压力测试，只是给观察留时间）。
    std::string padding(1024 * 1024, 'x');

    JSONConfigAdapter seed;
    seed.set_int("version", 1);
    seed.set_string("pad", padding);
    ASSERT_TRUE(seed.save_as(file.string()));

    JSONConfigAdapter adapter;
    ASSERT_TRUE(adapter.load(file.string()));
    adapter.set_int("version", 2);

    std::atomic<bool> done{false};
    std::atomic<int> reads{0};
    std::atomic<int> parse_failures{0};
    std::atomic<int> temp_observed{0};

    // 单独一个高频线程只盯临时文件：读 1 MiB 目标会占用若干毫秒，若和采样放在
    // 同一个循环里，可能整段「临时文件存在窗口」都被一次读取盖过去。
    std::thread temp_watcher([&] {
        while (!done.load(std::memory_order_relaxed)) {
            if (!list_temp_files(dir_).empty()) {
                temp_observed.fetch_add(1);
            }
            std::this_thread::yield();
        }
    });

    // 观察一：写入期间目标始终是可解析的完整 JSON，version 只能是旧值 1 或新值 2。
    std::thread reader([&] {
        while (!done.load(std::memory_order_relaxed)) {
            std::ifstream in(file, std::ios::binary);
            if (in) {
                std::string text((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
                try {
                    nlohmann::json parsed = nlohmann::json::parse(text);
                    int version = parsed.at("version").get<int>();
                    if (version != 1 && version != 2) {
                        parse_failures.fetch_add(1);
                    }
                    reads.fetch_add(1);
                } catch (const std::exception&) {
                    parse_failures.fetch_add(1);
                }
            }
            std::this_thread::yield();
        }
    });

    // 等观察者完成至少一次完整读取再写：高速机器上写入可能几毫秒就结束，
    // 不设这道屏障会让 reads 采样数变成 0，用例就失去了观察意义。
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (reads.load(std::memory_order_relaxed) == 0 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    ASSERT_GT(reads.load(), 0) << "观察者必须至少完成一次完整读取";

    bool saved = adapter.save();
    done.store(true, std::memory_order_relaxed);
    reader.join();
    temp_watcher.join();
    EXPECT_EQ(parse_failures.load(), 0) << "写入期间目标不能出现不可解析的半截内容";
    EXPECT_GT(temp_observed.load(), 0) << "应观察到与目标同目录的临时文件";
    EXPECT_TRUE(list_temp_files(dir_).empty()) << "无论替换成功或失败都不留临时文件";

    nlohmann::json final_json = nlohmann::json::parse(read_text(file));
    if (saved) {
        EXPECT_EQ(final_json["version"], 2) << "替换成功时目标应是完整的新内容";
    } else {
        // Windows 实测：目标若正被其他句柄打开，rename 覆盖会 sharing-violation 失败。
        // 这就是「替换阶段被中断」的真实形态 —— 目标必须仍是完整的旧内容。
        EXPECT_EQ(final_json["version"], 1) << "替换失败时目标必须保持完整的旧内容";
        EXPECT_EQ(final_json["pad"], padding) << "旧内容不能被截断";
    }
}

TEST_F(JSONConfigAdapterTest, SaveBeforeLoadFails) {
    JSONConfigAdapter adapter;
    adapter.set_string("key", "value");
    EXPECT_FALSE(adapter.save());
}

} // namespace
