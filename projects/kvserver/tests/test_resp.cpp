#include "tests/test_kit.h"
#include "src/server/resp.h"

TEST(resp, parse_command) {
    // 测试解析命令
    std::string input_buffer = "*2\r\n$4\r\nPING\r\n$4\r\nPONG\r\n";
    RESP_RESULT result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.muilti_bulk_length == 2);
    CHECK(result.commands.size() == 2);
    CHECK(result.commands[0] == "PING");
    CHECK(result.commands[1] == "PONG");
}

TEST(resp, resp_include_crlf) {
    // 测试解析包含 \r\n 的命令
    std::string input_buffer = "*1\r\n$7\r\nhi\r\nbye\r\n";
    RESP_RESULT result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.commands.size() == 1);
    CHECK(result.commands[0] == "hi\r\nbye");
}

TEST(resp, resp_null) {
    // 测试空串
    std::string input_buffer = "*1\r\n$0\r\n\r\n";
    RESP_RESULT result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.commands.size() == 1);
    CHECK(result.commands[0] == "");
}

TEST(resp, resp_half_packet) {
    // 测试半包
    std::string input_buffer = "*2\r\n$4\r\nPING\r\n$4\r\nPONG";
    RESP_RESULT result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);

    input_buffer = "*2\r\n";
    result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);

    input_buffer = "*3\r\n$3\r\nSET\r\n$3\r\nk\r\n";
    result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);

    input_buffer = "*1\r\n$4\r\nname";
    result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);
}

TEST(resp, resp_err) {
    // 测试错误包
    std::string input_buffer = "*2\r\n$4\r\nPING\r\n$4\r\nPONGxx";
    RESP_RESULT result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_ERR);
}