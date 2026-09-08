#include "src/server/resp.h"
#include "src/server/store.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <cstring>
#include <cerrno>
#include <string>

int main() {
    // socket()
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {
        std::cerr << "socket error: " << strerror(errno) << std::endl;
        return 1;
    }

    // bind()
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080); // 转大端
    addr.sin_addr.s_addr = INADDR_ANY;
    int bind_result = bind(socketfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (bind_result == -1) {
        std::cerr << "bind error: " << strerror(errno) << std::endl;
        return 1;
    }

    // listen()
    int queue_size = 0;
    int listen_result = listen(socketfd, queue_size);
    if (listen_result == -1) {
        std::cerr << "listen error: " << strerror(errno) << std::endl;
        return 1;
    } else {
        std::cout << "listening on port 8080..." << std::endl;
    }

    // 维护一个kv存储
    Store store;

    while(1) {
        // accept() 阻塞
        struct sockaddr_in client_addr; // 客户端地址
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socketfd = accept(socketfd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_len);
        if (client_socketfd == -1) {
            std::cerr << "accept error: " << strerror(errno) << std::endl;
            continue; // 继续等待下一个连接
        }

        std::cout << "Accepted a connection: " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;

        std::string querybuf; // 与客户端生命周期一致
        querybuf.reserve(4096); // 减少二次分配

        while (1) {
            // recv() 阻塞
            char buffer[1024]; // 临时 buffer ，用来接收一次发送的数据
            ssize_t bytes_received = recv(client_socketfd, buffer, sizeof(buffer), 0);
            if (bytes_received == -1) { // 出错
                std::cerr << "recv error from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << ": " << strerror(errno) << std::endl;
                break;
            } else if (bytes_received == 0) { // 客户端关闭连接
                break;
            }

            // 开始解析命令
            querybuf.append(buffer, bytes_received); // 追加接收结果
            std::cout << "From " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << " Received message: " << querybuf << std::endl;

            RESP_MULTI_RESULT result = parse_multi_command(querybuf); // 解析命令

            // 解析完毕开始发送结果
            // 构建返回结果
            std::string response;
            for (const auto& resp_result : result.commands) {
                response += build_reply(resp_result.argv, store);
            }

            // 解析出现错误，添加错误信息
            if (result.status == RESP_STATUS::RESP_ERR) {
                std::string error_msg = "ERR Protocol error: invalid request";
                response += encode_error(error_msg);
            }

            // 循环 send 确保发送完毕
            size_t total_sent = 0;
            size_t remaining = response.size();
            while (remaining > 0) {
                // send()
                ssize_t bytes_sent = send(client_socketfd, response.data()+total_sent, remaining, 0);
                if (bytes_sent == -1) {
                    std::cerr << "send error: " << strerror(errno) << std::endl;
                    break;
                }

                total_sent += bytes_sent;
                remaining -= bytes_sent;
            }
            if (remaining > 0) break; // send() 错误直接断开连接
            std::cout << "Sent response to " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << ": " << response << std::endl;

            // 解析出现错误，直接断开连接。
            if (result.status == RESP_STATUS::RESP_ERR) break;
        }

        // 关闭客户端套接字
        close(client_socketfd);
        std::cout << "Client disconnected: " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;
    }

    // 关闭服务器套接字
    close(socketfd);

    return 0;
}