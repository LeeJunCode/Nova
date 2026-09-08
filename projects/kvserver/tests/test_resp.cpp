#include "tests/test_kit.h"
#include "src/server/resp.h"
#include "src/server/store.h"

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

TEST(resp, resp_build_reply_ping_echo) {
    // 分发器测试直接喂词表:build_reply 收的是 argv,不经过 RESP 线帧。
    // {…} 花括号列表会构造一个临时 vector,绑定到 const 引用参数上。
    Store store;
    CHECK(build_reply({"PING"}, store) == "+PONG\r\n");
    CHECK(build_reply({"PING", "X"}, store) == "-ERR wrong number of arguments for 'ping' command\r\n");
    CHECK(build_reply({"ECHO", "hi"}, store) == "$2\r\nhi\r\n");
    CHECK(build_reply({"ECHO", ""}, store) == "$0\r\n\r\n");       // 值空串回 $0
    CHECK(build_reply({"EcHo", "Hi"}, store) == "$2\r\nHi\r\n");   // 命令名大小写不敏感
    CHECK(build_reply({"ECHO"}, store) == "-ERR wrong number of arguments for 'echo' command\r\n");
}

TEST(resp, resp_build_reply_set_get) {
    Store store;
    CHECK(build_reply({"set", "a", "b"}, store) == "+OK\r\n");
    CHECK(build_reply({"get", "a"}, store) == "$1\r\nb\r\n");

    // GET 缺键(null bulk $−1)与值空串($0)字节必须不同;缺键不是错误。
    CHECK(build_reply({"get", "nosuch"}, store) == "$-1\r\n");
    CHECK(build_reply({"set", "e", ""}, store) == "+OK\r\n");
    CHECK(build_reply({"get", "e"}, store) == "$0\r\n\r\n");

    // arity:set 严格 3 词、get 严格 2 词
    CHECK(build_reply({"set", "a"}, store) == "-ERR wrong number of arguments for 'set' command\r\n");
    CHECK(build_reply({"set", "a", "b", "c"}, store) == "-ERR wrong number of arguments for 'set' command\r\n");
    CHECK(build_reply({"get"}, store) == "-ERR wrong number of arguments for 'get' command\r\n");

    // 未知命令
    CHECK(build_reply({"frobnicate"}, store) == "-ERR unknown command 'frobnicate'\r\n");

    // 键区分大小写:A 与 a 是两个不同的 key
    CHECK(build_reply({"set", "A", "upper"}, store) == "+OK\r\n");
    CHECK(build_reply({"get", "A"}, store) == "$5\r\nupper\r\n");
    CHECK(build_reply({"get", "a"}, store) == "$1\r\nb\r\n");      // a 仍是最早 set 的 b
}

TEST(resp, resp_build_reply_via_parse) {
    // 模拟 server.cpp 真实路径:线帧 → parse_multi_command → 逐条 build_reply
    Store store;
    std::string buf = "*3\r\n$3\r\nSET\r\n$1\r\na\r\n$1\r\nb\r\n*2\r\n$3\r\nGET\r\n$1\r\na\r\n";
    RESP_MULTI_RESULT result = parse_multi_command(buf);
    CHECK(result.status == RESP_STATUS::RESP_OK);
    CHECK(result.commands.size() == 2); // 粘包:SET a b + GET a 两条一起榨干
    CHECK(build_reply(result.commands[0].argv, store) == "+OK\r\n");
    CHECK(build_reply(result.commands[1].argv, store) == "$1\r\nb\r\n");

    std::string buf2 = "*2\r\n$3\r\nGET\r\n$6\r\nnosuch\r\n";
    result = parse_multi_command(buf2);
    CHECK(result.commands.size() == 1);
    CHECK(build_reply(result.commands[0].argv, store) == "$-1\r\n"); // nosuch 没存过 → null bulk
}
