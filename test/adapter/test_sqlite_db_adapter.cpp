#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <system_error>

#include "SQLiteDBAdapter.h"

namespace {

using Prism::Adapter::AccountInfo;
using Prism::Adapter::AudioFileInfo;
using Prism::Adapter::SQLiteDBAdapter;
using Prism::Adapter::VideoFileInfo;

/**
 * @brief 生成进程内唯一的临时数据库路径（父目录一并建好）
 * @return 数据库文件路径
 */
std::filesystem::path make_db_path() {
    static std::mt19937_64 engine{std::random_device{}()};
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "prism-adapter-tests";
    std::filesystem::create_directories(dir);
    return dir / ("db-" + std::to_string(engine()) + ".sqlite3");
}

/**
 * @brief 造一条字段全非默认值的音频记录，用来验证每一列都真的落库
 * @return 音频文件信息
 */
AudioFileInfo make_audio_file() {
    AudioFileInfo info;
    info.title = "Clair de Lune";
    info.file_path = "D:/media/clair.flac";
    info.file_size = 33554432;
    info.cover_path = "D:/media/clair.jpg";
    info.artist = "Debussy";
    info.encoding_format = "FLAC";
    info.duration = 302000;
    info.bit_depth = 24;
    info.sample_rate = 96000;
    info.bit_rate = 2304000;
    info.channels = 2;
    info.channel_layout = "stereo";
    info.lyrics_path = "D:/media/clair.lrc";
    return info;
}

/**
 * @brief 造一条字段全非默认值的视频记录
 * @return 视频文件信息
 */
VideoFileInfo make_video_file() {
    VideoFileInfo info;
    info.filename = "demo.mp4";
    info.file_path = "D:/media/demo.mp4";
    info.file_size = 1073741824;
    info.author = "Prism";
    info.resolution_width = 1920;
    info.resolution_height = 1080;
    info.color_depth = 10;
    info.duration = 60000;
    info.encoding_format = "H.265";
    info.frame_rate = 59.94;
    info.bit_rate = 8000000;
    return info;
}

class SQLiteDBAdapterTest : public ::testing::Test {
  protected:
    void SetUp() override {
        path_ = make_db_path();
        ASSERT_TRUE(adapter_.init(path_.string()));
    }

    void TearDown() override {
        adapter_.close();
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    SQLiteDBAdapter adapter_;
    std::filesystem::path path_;
};

TEST_F(SQLiteDBAdapterTest, AccountRoundTrip) {
    AccountInfo account;
    account.username = "alice";
    account.email = "alice@example.com";
    account.password_hash = "e8d1e1850786";

    ASSERT_TRUE(adapter_.insert_account(account));

    auto found = adapter_.query_account_by_username("alice");
    ASSERT_TRUE(found.has_value());
    EXPECT_GT(found->id, 0);
    EXPECT_EQ(found->username, "alice");
    EXPECT_EQ(found->email, "alice@example.com");
    EXPECT_EQ(found->password_hash, "e8d1e1850786");

    EXPECT_FALSE(adapter_.query_account_by_username("bob").has_value());
}

TEST_F(SQLiteDBAdapterTest, DuplicateUsernameIsRejected) {
    AccountInfo account;
    account.username = "alice";
    account.email = "alice@example.com";
    account.password_hash = "hash";

    ASSERT_TRUE(adapter_.insert_account(account));
    EXPECT_FALSE(adapter_.insert_account(account));

    auto stored = adapter_.query_account_by_username("alice");
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->email, "alice@example.com");
}

TEST_F(SQLiteDBAdapterTest, AccountUpdateAndDelete) {
    AccountInfo account;
    account.username = "alice";
    account.email = "alice@example.com";
    account.password_hash = "hash";
    ASSERT_TRUE(adapter_.insert_account(account));

    auto stored = adapter_.query_account_by_username("alice");
    ASSERT_TRUE(stored.has_value());

    AccountInfo changed = *stored;
    changed.email = "alice@prism.dev";
    changed.password_hash = "newhash";
    ASSERT_TRUE(adapter_.update_account(changed));

    auto reloaded = adapter_.query_account_by_username("alice");
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->email, "alice@prism.dev");
    EXPECT_EQ(reloaded->password_hash, "newhash");

    ASSERT_TRUE(adapter_.delete_account(changed.id));
    EXPECT_FALSE(adapter_.query_account_by_username("alice").has_value());
    // 第二次删除影响 0 行，按契约返回 false。
    EXPECT_FALSE(adapter_.delete_account(changed.id));
}

TEST_F(SQLiteDBAdapterTest, AudioFileRoundTrip) {
    ASSERT_TRUE(adapter_.insert_audio_file(make_audio_file()));

    auto all = adapter_.query_audio_files();
    ASSERT_EQ(all.size(), 1U);

    auto file = adapter_.query_audio_file_by_id(all.front().id);
    ASSERT_TRUE(file.has_value());
    EXPECT_EQ(file->title, "Clair de Lune");
    EXPECT_EQ(file->file_path, "D:/media/clair.flac");
    EXPECT_EQ(file->file_size, 33554432U);
    EXPECT_EQ(file->cover_path, "D:/media/clair.jpg");
    EXPECT_EQ(file->artist, "Debussy");
    EXPECT_EQ(file->encoding_format, "FLAC");
    EXPECT_EQ(file->duration, 302000U);
    EXPECT_EQ(file->bit_depth, 24U);
    EXPECT_EQ(file->sample_rate, 96000U);
    EXPECT_EQ(file->bit_rate, 2304000U);
    EXPECT_EQ(file->channels, 2U);
    EXPECT_EQ(file->channel_layout, "stereo");
    EXPECT_EQ(file->lyrics_path, "D:/media/clair.lrc");

    EXPECT_FALSE(adapter_.query_audio_file_by_id(all.front().id + 1).has_value());
}

TEST_F(SQLiteDBAdapterTest, AudioFileUpdateAndDelete) {
    ASSERT_TRUE(adapter_.insert_audio_file(make_audio_file()));
    auto all = adapter_.query_audio_files();
    ASSERT_EQ(all.size(), 1U);

    AudioFileInfo changed = all.front();
    changed.title = "Clair de Lune (Remaster)";
    changed.sample_rate = 192000;
    ASSERT_TRUE(adapter_.update_audio_file(changed));

    auto reloaded = adapter_.query_audio_file_by_id(changed.id);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->title, "Clair de Lune (Remaster)");
    EXPECT_EQ(reloaded->sample_rate, 192000U);

    ASSERT_TRUE(adapter_.delete_audio_file(changed.id));
    EXPECT_TRUE(adapter_.query_audio_files().empty());
}

TEST_F(SQLiteDBAdapterTest, VideoFileRoundTrip) {
    ASSERT_TRUE(adapter_.insert_video_file(make_video_file()));

    auto all = adapter_.query_video_files();
    ASSERT_EQ(all.size(), 1U);

    auto file = adapter_.query_video_file_by_id(all.front().id);
    ASSERT_TRUE(file.has_value());
    EXPECT_EQ(file->filename, "demo.mp4");
    EXPECT_EQ(file->file_path, "D:/media/demo.mp4");
    EXPECT_EQ(file->file_size, 1073741824U);
    EXPECT_EQ(file->author, "Prism");
    EXPECT_EQ(file->resolution_width, 1920U);
    EXPECT_EQ(file->resolution_height, 1080U);
    EXPECT_EQ(file->color_depth, 10U);
    EXPECT_EQ(file->duration, 60000U);
    EXPECT_EQ(file->encoding_format, "H.265");
    EXPECT_DOUBLE_EQ(file->frame_rate, 59.94);
    EXPECT_EQ(file->bit_rate, 8000000U);

    ASSERT_TRUE(adapter_.delete_video_file(file->id));
    EXPECT_TRUE(adapter_.query_video_files().empty());
}

TEST_F(SQLiteDBAdapterTest, SettingsAreUpserted) {
    EXPECT_FALSE(adapter_.get_setting("audio.device").has_value());

    ASSERT_TRUE(adapter_.set_setting("audio.device", "wasapi"));
    EXPECT_EQ(adapter_.get_setting("audio.device"), std::string("wasapi"));

    ASSERT_TRUE(adapter_.set_setting("audio.device", "asio"));
    EXPECT_EQ(adapter_.get_setting("audio.device"), std::string("asio"));
}

TEST(SQLiteDBAdapter, InitRejectsEmptyPath) {
    SQLiteDBAdapter adapter;
    EXPECT_FALSE(adapter.init(""));
}

TEST(SQLiteDBAdapter, CallsBeforeInitAreRejected) {
    SQLiteDBAdapter adapter;
    AccountInfo account;
    account.username = "alice";
    account.email = "alice@example.com";
    account.password_hash = "hash";

    EXPECT_FALSE(adapter.insert_account(account));
    EXPECT_FALSE(adapter.query_account_by_username("alice").has_value());
    EXPECT_FALSE(adapter.set_setting("k", "v"));
    EXPECT_TRUE(adapter.query_audio_files().empty());
}

} // namespace
