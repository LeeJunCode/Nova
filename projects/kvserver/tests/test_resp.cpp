#include "tests/test_kit.h"
#include "src/server/resp.h"

TEST(resp, parse_command) {
    // 测试解析命令
    std::string input_buffer = "*2\r\n$4\r\nPING\r\n$4\r\nPONG\r\n";
    RESP_RESULT result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.multi_bulk_length == 2);
    CHECK(result.commands.size() == 2);
    CHECK(result.commands[0] == "PING");
    CHECK(result.commands[1] == "PONG");
}

TEST(resp, resp_command_include_crlf) {
    // 测试解析包含 \r\n 的命令
    std::string input_buffer = "*1\r\n$7\r\nhi\r\nbye\r\n";
    RESP_RESULT result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.commands.size() == 1);
    CHECK(result.commands[0] == "hi\r\nbye");
}

TEST(resp, resp_command_null) {
    // 测试空串
    std::string input_buffer = "*1\r\n$0\r\n\r\n";
    RESP_RESULT result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.commands.size() == 1);
    CHECK(result.commands[0] == "");
}

TEST(resp, resp_command_half_packet) {
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

TEST(resp, resp_command_err) {
    // 测试错误包
    std::string input_buffer = "*2\r\n$4\r\nPING\r\n$4\r\nPONGxx";
    RESP_RESULT result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_ERR);
}

TEST(resp, resp_command_stitch) {
    // 测试粘包
    std::string input_buffer = "*2\r\n$4\r\nPING\r\n$4\r\nPONG\r\n*1\r\n$4\r\nECHO\r\n";
    RESP_RESULT result = parse_command(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.multi_bulk_length == 2);
    CHECK(result.commands.size() == 2);
    CHECK(result.commands[0] == "PING");
    CHECK(result.commands[1] == "PONG");
    CHECK(result.offset == 24); // 粘包，已经解析的字节数
}

TEST(resp, resp_commands_two_command) {
    // 测试两条完整命令
    std::string input_buffer = "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nECHO\r\n";
    RESP_MULTI_RESULT result = parse_commands(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.commands.size() == 2);
    CHECK(result.commands[0].commands[0] == "PING");
    CHECK(result.commands[1].commands[0] == "ECHO");
    CHECK(input_buffer.empty());
}

TEST(resp, resp_commands_full_with_half) {
    // 一条完整+一条半包
    std::string input_buffer = "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nECHO";
    RESP_MULTI_RESULT result = parse_commands(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);
    CHECK(result.commands.size() == 1);
    CHECK(input_buffer == "*1\r\n$4\r\nECHO");
}

TEST(resp, resp_commands_half) {
    //纯半包
    std::string input_buffer = "*1\r\n$4\r\nECHO";
    RESP_MULTI_RESULT result = parse_commands(input_buffer);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);
    CHECK(result.commands.size() == 0);
    CHECK(input_buffer == "*1\r\n$4\r\nECHO");
}
