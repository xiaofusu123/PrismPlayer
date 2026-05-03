#include <gtest/gtest.h>
#include <asio.hpp>
#include <libavutil/version.h>

TEST(AAsioTest, Version) {
    int version = asio_version();
    EXPECT_GE(version, ASIO_VERSION_INT(1, 0, 0));
}
