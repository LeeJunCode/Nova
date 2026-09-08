#include "src/server/resp.h"
#include "src/server/store.h"

#include <climits>
#include <cctype>

int str_to_int(const std::string& str) {
    int result = 0;
    for (char c : str) {
        if (c < '0' || c > '9') {
            return -1;
        }
        if (result > INT_MAX / 10) { // 防止溢出
            return -1;
        } else if (result == INT_MAX / 10 && c - '0' > INT_MAX % 10) { // 防止溢出
            return -1;
        }
        result = result * 10 + (c - '0');
    }
    return result;
}

RESP_RESULT parse_command(const std::string& querybuf) {
    // 初始化解析结果
    RESP_RESULT resp_result;
    resp_result.status = RESP_STATUS::RESP_OK;
    resp_result.argv.clear();
    resp_result.argc = 0;
    resp_result.offset = 0;

    // 空 buffer，返回错误
    if (querybuf.empty()) {
        resp_result.status = RESP_STATUS::RESP_ERR;
        return resp_result;
    }

    // 开始解析 RESP 协议:一条命令 = 一个顶层数组
    std::string::const_iterator it = querybuf.cbegin();
    if (*it != '*') { // 开头格式错误
        resp_result.status = RESP_STATUS::RESP_ERR;
        return resp_result;
    }
    // 读 *N 头部的 N,即这条命令的词个数 argc(含命令名本身)
    if (querybuf.find("\r\n") == std::string::npos) { // 没有找到换行符，可能是半包
        resp_result.status = RESP_STATUS::RESP_HALF_PACKET;
        return resp_result;
    }
    std::string argc_str = querybuf.substr(1, querybuf.find("\r\n") - 1);
    resp_result.argc = str_to_int(argc_str);
    // argc 非法:0 = 空数组(没有命令名),-1 = 头部不是数字
    if (resp_result.argc == -1 || resp_result.argc == 0) {
        resp_result.status = RESP_STATUS::RESP_ERR;
        return resp_result;
    }
    // 词个数超过上限
    if (resp_result.argc > ARGC_MAX) {
        resp_result.status = RESP_STATUS::RESP_ERR;
        return resp_result;
    }
    it += argc_str.length() + 3; // 跳过 *<N>\r\n

    int remaining = resp_result.argc; // 还差几个词没读
    while (remaining) {
        if (it == querybuf.cend()) { // 到达 buffer 末尾，可能是半包
            resp_result.status = RESP_STATUS::RESP_HALF_PACKET;
            return resp_result;
        }
        // 逐个读取元素(bulk string),填进 argv
        if (*it == '$') {
            // 读 $<长度>
            if (querybuf.find("\r\n", it - querybuf.cbegin()) == std::string::npos) { // 没有找到换行符，可能是半包
                resp_result.status = RESP_STATUS::RESP_HALF_PACKET;
                return resp_result;
            }
            std::string bulk_length_str = querybuf.substr(it - querybuf.cbegin() + 1, querybuf.find("\r\n", it - querybuf.cbegin()) - (it - querybuf.cbegin() + 1));
            int bulk_length = str_to_int(bulk_length_str);
            if (bulk_length == -1) {
                resp_result.status = RESP_STATUS::RESP_ERR;
                return resp_result;
            }
            if (bulk_length > BULK_STRING_LENGTH_MAX) { // 词内容长度超过上限
                resp_result.status = RESP_STATUS::RESP_ERR;
                return resp_result;
            }
            it += bulk_length_str.length() + 3; // 跳过 $<length>\r\n

            // 取词内容
            if (querybuf.length() < (it - querybuf.cbegin()) + bulk_length + 2) { // 内容不完整，可能是半包
                resp_result.status = RESP_STATUS::RESP_HALF_PACKET;
                return resp_result;
            }
            std::string arg = querybuf.substr(it - querybuf.cbegin(), bulk_length);
            it += bulk_length; // 跳过 <arg>
            if (querybuf.substr(it - querybuf.cbegin(), 2) != "\r\n") { // 词后没有跟 \r\n，错误
                resp_result.status = RESP_STATUS::RESP_ERR;
                return resp_result;
            }
            resp_result.argv.push_back(arg);
            it += 2; // 跳过 \r\n
            --remaining;
        } else {
            resp_result.status = RESP_STATUS::RESP_ERR;
            return resp_result;
        }
    }

    // 有可能粘包，记录已经解析的字节
    resp_result.offset = it - querybuf.cbegin();

    return resp_result;
}

RESP_MULTI_RESULT parse_multi_command(std::string& querybuf) {
    // 初始化结果
    RESP_MULTI_RESULT resp_multi_result;

    // 循环榨干 querybuf 里所有完整命令
    while (!querybuf.empty()) {
        RESP_RESULT resp_result = parse_command(querybuf);
        resp_multi_result.status = resp_result.status;

        if (resp_result.status != RESP_STATUS::RESP_OK) {
            break;
        }

        resp_multi_result.commands.push_back(resp_result);

        // 切掉 querybuf 中解析成功的这条命令的字节
        querybuf.erase(querybuf.begin(), querybuf.begin() + resp_multi_result.commands.back().offset);
    }

    return resp_multi_result;
}

// 编码器
std::string encode_bulk_string(const std::string& bulk) {
    std::string size_str = std::to_string(bulk.size()); // 获取这一个词的大小
    std::string encode_bulk;
    encode_bulk.reserve(size_str.size() + bulk.size() + 5); // 预分配内存

    encode_bulk.push_back('$');
    encode_bulk += size_str;
    encode_bulk += "\r\n";
    encode_bulk += bulk;
    encode_bulk += "\r\n";

    return encode_bulk;
}

// 分发器
std::string build_reply(const std::vector<std::string>& argv, Store& store) {
    // 转小写
    auto to_lower = [] (std::string str) {
        for (auto& c : str) {
            c = std::tolower(static_cast<unsigned char>(c));
        }
        return str;
    };
    std::string command_name = to_lower(argv[0]); // 获取命令名

    if (command_name == "echo") {
        if (argv.size() != 2) {
            std::string wrong_num_msg = "ERR wrong number of arguments for '" + command_name + "' command";
            return encode_error(wrong_num_msg);
        }
        return encode_bulk_string(argv[1]);
    } else if (command_name == "ping") {
        if (argv.size() != 1) {
            std::string wrong_num_msg = "ERR wrong number of arguments for '" + command_name + "' command";
            return encode_error(wrong_num_msg);
        }
        std::string pong("PONG");
        return encode_simple_string(pong);
    } else if (command_name == "get") {
        if (argv.size() != 2) {
            std::string wrong_num_msg = "ERR wrong number of arguments for '" + command_name + "' command";
            return encode_error(wrong_num_msg);
        }
        std::string value;
        if (store.get(argv[1], value)) {
            return encode_bulk_string(value);
        } else {
            return encode_null_bulk();
        }
    } else if (command_name == "set") {
        if (argv.size() != 3) {
            std::string wrong_num_msg = "ERR wrong number of arguments for '" + command_name + "' command";
            return encode_error(wrong_num_msg);
        }
        store.set(argv[1], argv[2]);
        std::string ok("OK");
        return encode_simple_string(ok);
    } else {
        std::string unknown_msg = "ERR unknown command '" + command_name + "'";
        return encode_error(unknown_msg);
    }
}
