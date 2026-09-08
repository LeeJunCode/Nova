#include "tests/test_kit.h"
#include "src/server/resp.h"
#include "src/server/store.h"

#include <climits>
#include <string>

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

TEST(resp, resp_build_reply_del_exists) {
    Store store;
    // 准备数据:两个普通键 + 一个空串键
    CHECK(build_reply({"set", "a", "1"}, store) == "+OK\r\n");
    CHECK(build_reply({"set", "b", "2"}, store) == "+OK\r\n");
    CHECK(build_reply({"set", "e", ""}, store) == "+OK\r\n");

    // EXISTS:存在→:1,缺键→:0(缺键不是错误)
    CHECK(build_reply({"exists", "a"}, store) == ":1\r\n");
    CHECK(build_reply({"exists", "nosuch"}, store) == ":0\r\n");
    CHECK(build_reply({"exists", "e"}, store) == ":1\r\n"); // 值空串但键存在,照样 :1

    // EXISTS 可变参数:数每个词的出现次数,同一 key 出现两次算两次
    CHECK(build_reply({"exists", "a", "nosuch", "b"}, store) == ":2\r\n");
    CHECK(build_reply({"exists", "a", "a"}, store) == ":2\r\n");

    // DEL 可变参数:只数真正删掉的
    CHECK(build_reply({"del", "a", "nosuch", "b"}, store) == ":2\r\n");
    // 上面删掉了 a 和 b,再查都存在 → :0
    CHECK(build_reply({"exists", "a", "b"}, store) == ":0\r\n");
    CHECK(build_reply({"del", "a"}, store) == ":0\r\n");     // 缺键不是错误
    CHECK(build_reply({"del", "e"}, store) == ":1\r\n");     // e 之前没删过
    CHECK(build_reply({"del", "a", "a"}, store) == ":0\r\n"); // 同一 key 重复,已删则只算一次、共 0

    // arity:del/exists 至少 2 词(命令名 + 至少一个 key)
    CHECK(build_reply({"del"}, store) == "-ERR wrong number of arguments for 'del' command\r\n");
    CHECK(build_reply({"exists"}, store) == "-ERR wrong number of arguments for 'exists' command\r\n");

    // 删掉后 GET 也读不到了 → null bulk
    CHECK(build_reply({"get", "e"}, store) == "$-1\r\n");
}

TEST(resp, resp_build_reply_incr_family) {
    Store store;

    // 缺键按 0 起步,回 :N;写回后可连续累加
    CHECK(build_reply({"incr", "k1"}, store) == ":1\r\n");
    CHECK(build_reply({"incr", "k1"}, store) == ":2\r\n");
    CHECK(build_reply({"decr", "k1"}, store) == ":1\r\n");
    CHECK(build_reply({"decr", "k2"}, store) == ":-1\r\n"); // 缺键 DECR = 0-1

    // 正/负增量:INCRBY/DECRBY 的 delta 都允许负(负增量即反向运算)
    CHECK(build_reply({"set", "k3", "5"}, store) == "+OK\r\n");
    CHECK(build_reply({"incrby", "k3", "7"}, store) == ":12\r\n");
    CHECK(build_reply({"incrby", "k3", "-3"}, store) == ":9\r\n");
    CHECK(build_reply({"decrby", "k3", "4"}, store) == ":5\r\n");
    CHECK(build_reply({"decrby", "k3", "-5"}, store) == ":10\r\n"); // 5-(-5)=+5
    CHECK(build_reply({"get", "k3"}, store) == "$2\r\n10\r\n");     // 写回的是规范十进制,GET 读回

    // 值不是整数 → 合并错误句
    CHECK(build_reply({"set", "kbad", "notanum"}, store) == "+OK\r\n");
    CHECK(build_reply({"incr", "kbad"}, store) == "-ERR value is not an integer or out of range\r\n");
    CHECK(build_reply({"incrby", "kbad", "1"}, store) == "-ERR value is not an integer or out of range\r\n");
    // 增量不是整数 → 同一句;缺键 + 非法增量 → 增量非法
    CHECK(build_reply({"incrby", "kbad", "abc"}, store) == "-ERR value is not an integer or out of range\r\n");
    CHECK(build_reply({"incrby", "kmis", "abc"}, store) == "-ERR value is not an integer or out of range\r\n");

    // arity:incr/decr 恰好 2 词,incrby/decrby 恰好 3 词
    CHECK(build_reply({"incr"}, store) == "-ERR wrong number of arguments for 'incr' command\r\n");
    CHECK(build_reply({"incr", "k", "x"}, store) == "-ERR wrong number of arguments for 'incr' command\r\n");
    CHECK(build_reply({"incrby", "k"}, store) == "-ERR wrong number of arguments for 'incrby' command\r\n");
    CHECK(build_reply({"decrby", "k", "1", "2"}, store) == "-ERR wrong number of arguments for 'decrby' command\r\n");

    // 溢出边界:顶/底都不能静默回绕
    const std::string MAX = std::to_string(LLONG_MAX);
    const std::string MIN = std::to_string(LLONG_MIN);
    // +1 顶穿 LLONG_MAX
    CHECK(build_reply({"set", "khi", MAX}, store) == "+OK\r\n");
    CHECK(build_reply({"incr", "khi"}, store) == "-ERR increment or decrement would overflow\r\n");
    // 合法回撤一格再顶
    CHECK(build_reply({"incrby", "khi", "-1"}, store) == ":" + std::to_string(LLONG_MAX - 1) + "\r\n");
    CHECK(build_reply({"incr", "khi"}, store) == ":" + MAX + "\r\n");
    CHECK(build_reply({"incr", "khi"}, store) == "-ERR increment or decrement would overflow\r\n");
    // DECRBY 负增量 = 加 |delta|,同样顶穿
    CHECK(build_reply({"set", "khi2", MAX}, store) == "+OK\r\n");
    CHECK(build_reply({"decrby", "khi2", "-1"}, store) == "-ERR increment or decrement would overflow\r\n");
    // 底侧:LLONG_MIN -1(下溢)
    CHECK(build_reply({"set", "klo", MIN}, store) == "+OK\r\n");
    CHECK(build_reply({"decr", "klo"}, store) == "-ERR increment or decrement would overflow\r\n");
    CHECK(build_reply({"decrby", "klo", "1"}, store) == "-ERR increment or decrement would overflow\r\n");
    CHECK(build_reply({"set", "klo2", MIN}, store) == "+OK\r\n");
    CHECK(build_reply({"incrby", "klo2", "-1"}, store) == "-ERR increment or decrement would overflow\r\n");
    // MIN -(-1) = MIN+1 合法
    CHECK(build_reply({"set", "klo3", MIN}, store) == "+OK\r\n");
    CHECK(build_reply({"decrby", "klo3", "-1"}, store) == ":" + std::to_string(LLONG_MIN + 1) + "\r\n");

    // 增量本身越出 64 位 → 解析非法(值不是整数/越界那句)
    CHECK(build_reply({"incrby", "k", "9223372036854775808"}, store) == "-ERR value is not an integer or out of range\r\n");
    CHECK(build_reply({"incrby", "k", "-9223372036854775809"}, store) == "-ERR value is not an integer or out of range\r\n");
}
