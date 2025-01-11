#include <gtest/gtest.h>
#include "../code/buffer/buffer.h"

class BufferTest : public ::testing::Test {
protected:
    Buffer buffer;

    BufferTest() : buffer(1024) {}  // 初始化一个大小为1024的缓冲区
};

// 测试基本Append操作
TEST_F(BufferTest, AppendBasic) {
    buffer.Append("Hello", 5);
    ASSERT_EQ(buffer.ReadableBytes(), 5);
    ASSERT_EQ(std::string(buffer.Peek(), buffer.ReadableBytes()), "Hello");
}

// 测试扩展缓冲区的功能
TEST_F(BufferTest, EnsureWriteable) {
    buffer.EnsureWriteable(1024);  // 确保可写空间为1024
    ASSERT_GE(buffer.size(), 1024);
}

// 测试缓冲区扩展导致的std::length_error异常
TEST_F(BufferTest, LengthError) {
    try {
        buffer.EnsureWriteable(65536);  // 超过缓冲区容量的最大值
        FAIL() << "Expected std::length_error";
    } catch (const std::length_error& e) {
        EXPECT_EQ(std::string(e.what()), "Buffer size exceeds maximum capacity");
    } catch (...) {
        FAIL() << "Expected std::length_error";
    }
}

// 测试读取和写入操作
TEST_F(BufferTest, ReadWrite) {
    buffer.Append("Hello", 5);
    char temp[6] = {0};
    ssize_t bytesRead = buffer.WriteFd(1, nullptr);  // 将内容写入标准输出
    ASSERT_EQ(bytesRead, 5);
    ASSERT_EQ(std::string(temp), "Hello");
    ASSERT_EQ(buffer.ReadableBytes(), 0);  // 已读取掉所有内容
}

// 测试分散读取的正确性
TEST_F(BufferTest, Readv) {
    buffer.Append("Hello, world!", 13);
    int saveErrno;
    ssize_t bytesRead = buffer.ReadFd(STDIN_FILENO, &saveErrno);  // 使用标准输入来模拟数据读取
    ASSERT_EQ(bytesRead, 13);
    ASSERT_EQ(std::string(buffer.Peek(), buffer.ReadableBytes()), "Hello, world!");
}

// 测试缩小缓冲区容量
TEST_F(BufferTest, ShrinkToFit) {
    buffer.Append("Hello", 5);
    buffer.MakeSpace_(0);  // 强制缩小缓冲区
    ASSERT_EQ(buffer.size(), 5);
    ASSERT_EQ(std::string(buffer.Peek(), buffer.ReadableBytes()), "Hello");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
