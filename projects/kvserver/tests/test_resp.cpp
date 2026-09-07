#include "tests/test_kit.h"
#include "src/server/resp.h"

TEST(resp, parse_command) {
    // 测试解析命令:一个数组 = 一条命令 = 两个词
    std::string querybuf = "*2\r\n$4\r\nPING\r\n$4\r\nPONG\r\n";
    RESP_RESULT result = parse_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.argc == 2);
    CHECK(result.argv.size() == 2);
    CHECK(result.argv[0] == "PING");
    CHECK(result.argv[1] == "PONG");
}

TEST(resp, resp_command_include_crlf) {
    // 测试解析包含 \r\n 的命令
    std::string querybuf = "*1\r\n$7\r\nhi\r\nbye\r\n";
    RESP_RESULT result = parse_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.argv.size() == 1);
    CHECK(result.argv[0] == "hi\r\nbye");
}

TEST(resp, resp_command_null) {
    // 测试空串
    std::string querybuf = "*1\r\n$0\r\n\r\n";
    RESP_RESULT result = parse_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.argv.size() == 1);
    CHECK(result.argv[0] == "");
}

TEST(resp, resp_command_half_packet) {
    // 测试半包
    std::string querybuf = "*2\r\n$4\r\nPING\r\n$4\r\nPONG";
    RESP_RESULT result = parse_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);

    querybuf = "*2\r\n";
    result = parse_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);

    querybuf = "*3\r\n$3\r\nSET\r\n$3\r\nk\r\n";
    result = parse_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);

    querybuf = "*1\r\n$4\r\nname";
    result = parse_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);
}

TEST(resp, resp_command_err) {
    // 测试错误包
    std::string querybuf = "*2\r\n$4\r\nPING\r\n$4\r\nPONGxx";
    RESP_RESULT result = parse_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_ERR);
}

TEST(resp, resp_command_stitch) {
    // 测试粘包
    std::string querybuf = "*2\r\n$4\r\nPING\r\n$4\r\nPONG\r\n*1\r\n$4\r\nECHO\r\n";
    RESP_RESULT result = parse_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.argc == 2);
    CHECK(result.argv.size() == 2);
    CHECK(result.argv[0] == "PING");
    CHECK(result.argv[1] == "PONG");
    CHECK(result.offset == 24); // 粘包，已经解析的字节数
}

TEST(resp, resp_commands_two_command) {
    // 测试两条完整命令
    std::string querybuf = "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nECHO\r\n";
    RESP_MULTI_RESULT result = parse_multi_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.commands.size() == 2);
    CHECK(result.commands[0].argv[0] == "PING");
    CHECK(result.commands[1].argv[0] == "ECHO");
    CHECK(querybuf.empty());
}

TEST(resp, resp_commands_full_with_half) {
    // 一条完整+一条半包
    std::string querybuf = "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nECHO";
    RESP_MULTI_RESULT result = parse_multi_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);
    CHECK(result.commands.size() == 1);
    CHECK(querybuf == "*1\r\n$4\r\nECHO");
}

TEST(resp, resp_commands_half) {
    //纯半包
    std::string querybuf = "*1\r\n$4\r\nECHO";
    RESP_MULTI_RESULT result = parse_multi_command(querybuf);
    CHECK(result.status == RESP_STATUS::RESP_HALF_PACKET);
    CHECK(result.commands.size() == 0);
    CHECK(querybuf == "*1\r\n$4\r\nECHO");
}

TEST(resp, resp_build_reply) {
    std::string querybuf = "*1\r\n$4\r\nPING\r\n"; // "PING"
    RESP_MULTI_RESULT result = parse_multi_command(querybuf);
    std::string reply = build_reply(result.commands[0].argv);
    CHECK(reply == "+PONG\r\n");

    querybuf = "*2\r\n$4\r\nPING\r\n$1\r\nX\r\n"; // "PING X"
    result = parse_multi_command(querybuf);
    reply = build_reply(result.commands[0].argv);
    CHECK(reply == "-ERR wrong number of arguments for 'ping' command\r\n");

    querybuf = "*1\r\n$4\r\nECHO\r\n"; // "ECHO"
    result = parse_multi_command(querybuf);
    reply = build_reply(result.commands[0].argv);
    CHECK(reply == "-ERR wrong number of arguments for 'echo' command\r\n");

    querybuf = "*2\r\n$4\r\nECHO\r\n$2\r\nhi\r\n"; // "ECHO hi"
    result = parse_multi_command(querybuf);
    reply = build_reply(result.commands[0].argv);
    CHECK(reply == "$2\r\nhi\r\n");

    querybuf = "*2\r\n$4\r\nECHO\r\n$0\r\n\r\n"; // "ECHO "
    result = parse_multi_command(querybuf);
    reply = build_reply(result.commands[0].argv);
    CHECK(reply == "$0\r\n\r\n");

    querybuf = "*3\r\n$3\r\nset\r\n$1\r\na\r\n$1\r\nb\r\n"; // "set a b"
    result = parse_multi_command(querybuf);
    reply = build_reply(result.commands[0].argv);
    CHECK(reply == "-ERR unknown command 'set'\r\n");

    querybuf = "*2\r\n$4\r\nEcHo\r\n$2\r\nHi\r\n"; // "EcHo Hi"
    result = parse_multi_command(querybuf);
    reply = build_reply(result.commands[0].argv);
    CHECK(reply == "$2\r\nHi\r\n");
}
