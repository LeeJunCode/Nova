#include "src/server/resp.h"
#include <climits>

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

RESP_RESULT parse_command(const std::string& input_buffer) {
    // 初始化解析结果
    RESP_RESULT resp_result;
    resp_result.status = RESP_STATUS::RESP_OK;
    resp_result.commands.clear();
    resp_result.multi_bulk_length = 0;
    resp_result.offset = 0;

    // 空 buffer，返回错误
    if (input_buffer.empty()) {
        resp_result.status = RESP_STATUS::RESP_ERR;
        return resp_result;
    }

    // 开始解析 RESP 协议
    std::string::const_iterator it = input_buffer.cbegin();
    if (*it != '*') { // 开头格式错误
        resp_result.status = RESP_STATUS::RESP_ERR;
        return resp_result;
    }
    // 获取命令条数
    if (input_buffer.find("\r\n") == std::string::npos) { // 没有找到换行符，可能是半包
        resp_result.status = RESP_STATUS::RESP_HALF_PACKET;
        return resp_result;
    }
    std::string multi_bulk_length_str = input_buffer.substr(1, input_buffer.find("\r\n") - 1);
    resp_result.multi_bulk_length = str_to_int(multi_bulk_length_str);
    // 检查命令条数是否合法
    if (resp_result.multi_bulk_length == -1 || resp_result.multi_bulk_length == 0) {
        resp_result.status = RESP_STATUS::RESP_ERR;
        return resp_result;
    }
    // 检查命令条数是否超过最大值
    if (resp_result.multi_bulk_length > MULTI_BULK_LENGTH_MAX) {
        resp_result.status = RESP_STATUS::RESP_ERR;
        return resp_result;
    }
    it += multi_bulk_length_str.length() + 3; // 跳过 *<number_of_elements>\r\n

    int command_count = resp_result.multi_bulk_length;
    while (command_count) {
        if (it == input_buffer.cend()) { // 到达 buffer 末尾，可能是半包
            resp_result.status = RESP_STATUS::RESP_HALF_PACKET;
            return resp_result;
        }
        // 解析每个命令
        if (*it == '$') {
            // 获取命令长度
            if (input_buffer.find("\r\n", it - input_buffer.cbegin()) == std::string::npos) { // 没有找到换行符，可能是半包
                resp_result.status = RESP_STATUS::RESP_HALF_PACKET;
                return resp_result;
            }
            std::string bulk_length_str = input_buffer.substr(it - input_buffer.cbegin() + 1, input_buffer.find("\r\n", it - input_buffer.cbegin()) - (it - input_buffer.cbegin() + 1));
            int bulk_length = str_to_int(bulk_length_str);
            if (bulk_length == -1) {
                resp_result.status = RESP_STATUS::RESP_ERR;
                return resp_result;
            }
            if (bulk_length > BULK_STRING_LENGTH_MAX) { // 命令长度超过最大值
                resp_result.status = RESP_STATUS::RESP_ERR;
                return resp_result;
            }
            it += bulk_length_str.length() + 3; // 跳过 $<length>\r\n

            // 获取命令内容
            if (input_buffer.length() < (it - input_buffer.cbegin()) + bulk_length + 2) { // 命令内容不完整，可能是半包
                resp_result.status = RESP_STATUS::RESP_HALF_PACKET;
                return resp_result;
            }
            std::string command = input_buffer.substr(it - input_buffer.cbegin(), bulk_length);
            it += bulk_length; // 跳过 <command>
            if (input_buffer.substr(it - input_buffer.cbegin(), 2) != "\r\n") { // 命令结束后没有找到换行符，错误
                resp_result.status = RESP_STATUS::RESP_ERR;
                return resp_result;
            }
            resp_result.commands.push_back(command);
            it += 2; // 跳过 \r\n
            --command_count;
        } else {
            resp_result.status = RESP_STATUS::RESP_ERR;
            return resp_result;
        }
    }
    if (command_count != 0) { // 命令条数不匹配，可能是半包
        resp_result.status = RESP_STATUS::RESP_HALF_PACKET;
        return resp_result;
    }

    // 有可能粘包，记录已经解析的字节
    resp_result.offset = it - input_buffer.cbegin();

    return resp_result;
}

RESP_MULTI_RESULT parse_commands(std::string& input_buffer) {
    // 初始化结果
    RESP_MULTI_RESULT resp_multi_result;

    // 开始解析命令
    while (!input_buffer.empty()) {
        RESP_RESULT resp_result = parse_command(input_buffer);
        resp_multi_result.status = resp_result.status;

        if (resp_result.status != RESP_STATUS::RESP_OK) {
            break;
        }

        resp_multi_result.commands.push_back(resp_result);

        // 切掉 input_buffer 中解析成功的命令的字节
        input_buffer.erase(input_buffer.begin(), input_buffer.begin() + resp_multi_result.commands.back().offset);
    }

    return resp_multi_result;
}
