#pragma once
#include <string>
#include <vector>

// 解析一次请求的状态。协议里"一条命令" = 一个顶层 *N 数组。
enum RESP_STATUS {
    RESP_OK = 1, // 解析完成,拿到一条完整命令
    RESP_HALF_PACKET = 0, // 半包
    RESP_ERR = -1,
};

// 单条命令的解析结果。
struct RESP_RESULT {
    RESP_STATUS status{RESP_OK};
    std::vector<std::string> argv; // 这条命令的词:argv[0]=命令名,argv[1..]=参数(解析层只切词、不分词义)
    long long argc{0}; // 命令含有的词个数
    size_t offset{0}; // 本条命令占的字节数,仅 status==OK 时有效;若 != querybuf.length() 说明后面还有(粘包)
};

// 多条命令
struct RESP_MULTI_RESULT {
    RESP_STATUS status{RESP_OK};
    std::vector<RESP_RESULT> commands; // 每个元素 = 一条已解析的完整命令
};

#define ARGC_MAX 32 // 单条命令的词个数上限
#define BULK_STRING_LENGTH_MAX 1024 // 单个词长度上限

bool parse_ll(const std::string& s, long long& out);

// 解析一条完整命令
RESP_RESULT parse_command(const std::string& querybuf);

// 解析缓冲头全部命令
RESP_MULTI_RESULT parse_multi_command(std::string& querybuf);

// 编码器
// 对一个词编码
std::string encode_bulk_string(const std::string& bulk);
// 对一个简单字符串编码
inline std::string encode_simple_string(const std::string& simple) {
    return "+" + simple + "\r\n";
}
// 对错误编码
inline std::string encode_error(const std::string& msg) {
    return "-" + msg + "\r\n";
}
// 对空词编码
inline std::string encode_null_bulk() {
    return "$-1\r\n";
}
// 对整数编码
std::string encode_integer(const long long number);

class Store; // build_reply 要用到存储，前向声明一下
// 分发器
std::string build_reply(const std::vector<std::string>& argv, Store& store);
