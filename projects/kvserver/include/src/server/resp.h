#pragma once
#include <string>
#include <vector>

enum RESP_STATUS {
    RESP_OK = 1, // 解析完成
    RESP_HALF_PACKET = 0, // 半包
    RESP_ERR = -1, // 解析错误
};

enum RESP_SYMBOLS {
    RESP_STRING, // +<string>\r\n
    RESP_ERROR,  // -<error>\r\n
    RESP_INTEGER, // :<integer>\r\n
    RESP_BULK_STRING, // $<length>\r\n<string>\r\n
    RESP_ARRAY // *<number_of_elements>\r\n<element_1><element_2>...<element_N>
};

struct RESP_RESULT {
    RESP_STATUS status{RESP_OK};
    std::vector<std::string> commands;
    int muilti_bulk_length{0}; // 命令条数
};

#define MULTI_BULK_LENGTH_MAX 32 // 命令条数最大值
#define BULK_STRING_LENGTH_MAX 1024 // 命令长度最大值

int str_to_int(const std::string& str);
RESP_RESULT parse_command(const std::string& input_buffer);