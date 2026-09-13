#include "src/server/resp.h"
#include "src/server/store.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include <iostream>
#include <cstring>
#include <cerrno>
#include <string>
#include <unordered_map>

struct Client {
    std::string querybuf; // 接收的数据
    std::string outbuf; // 待发送的数据
    std::string peer; // 客户端地址

    Client(std::string q, std::string c) : querybuf(q), peer(c) {}
};

void close_client(int client_socketfd, int epfd, std::unordered_map<int, Client>& connections) {
    auto it = connections.find(client_socketfd);
    if (it == connections.end()) {
        return; // 已经关过了
    }
    std::cout << "Client disconnected: " << it->second.peer << std::endl;

    if (epoll_ctl(epfd, EPOLL_CTL_DEL, client_socketfd, NULL) == -1) {
        std::cerr << "epoll delete fd error: " << std::error_code(errno, std::generic_category()).message() << std::endl;
    }

    connections.erase(client_socketfd);
    close(client_socketfd);
}

bool flush(int client_socketfd, int epfd, std::unordered_map<int, Client>& connections) {
    std::string& outbuf = connections.at(client_socketfd).outbuf;
    struct epoll_event ev;
    ev.events = EPOLLIN; // 可以读的时候继续通知
    ev.data.fd = client_socketfd;
    while (outbuf.size()) {
        ssize_t n = send(client_socketfd, outbuf.data(), outbuf.size(), MSG_NOSIGNAL);
        if (n > 0) { // 正常发送，继续发
            outbuf.erase(0, n); // 截断已经发送的数据
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // 缓冲区满了，不能发了
                ev.events = ev.events | EPOLLOUT; // 可以写的时候继续通知
                break;
            } else if (errno == EINTR) { // 被打断重传
                continue;
            } else { // 出错
                close_client(client_socketfd, epfd, connections);
                return false;
            }
        }
    }

    if (epoll_ctl(epfd, EPOLL_CTL_MOD, client_socketfd, &ev) == -1) {
        std::cerr << "epoll MOD fd error: " << std::error_code(errno, std::generic_category()).message() << std::endl;
    }

    return true;
}

void handle_read(int client_socketfd, Store& store, int epfd, std::unordered_map<int, Client>& connections) {
    std::string& querybuf = connections.at(client_socketfd).querybuf;
    const std::string& peer = connections.at(client_socketfd).peer;

    while (1) {
        // recv() 非阻塞
        char buffer[1024]; // 临时 buffer ，用来接收一次发送的数据
        ssize_t bytes_received = recv(client_socketfd, buffer, sizeof(buffer), 0);
        if (bytes_received == 0) { // 客户端关闭连接
            close_client(client_socketfd, epfd, connections);
            return;
        } else if (bytes_received == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // 数据读完了，此次事件完成,直接返回
                return ;
            } else if (errno == EINTR) { // 被打断，重试
                continue;
            } else { // 出错
                std::cerr << "recv error from " << peer << ": " << std::error_code(errno, std::generic_category()).message() << std::endl;
                close_client(client_socketfd, epfd, connections);
                return;
            }
        }

        // 开始解析命令
        querybuf.append(buffer, bytes_received); // 追加接收结果
        std::cout << "From " << peer << " Received message: " << querybuf << std::endl;

        RESP_MULTI_RESULT result = parse_multi_command(querybuf); // 解析命令

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

        // 一律进待发队列
        connections.at(client_socketfd).outbuf += response;
        if (!flush(client_socketfd, epfd, connections)) { // 发送命令结果
            return; // flush 关闭了连接
        }

        // 解析出现错误，断开连接。
        if (result.status == RESP_STATUS::RESP_ERR) {
            close_client(client_socketfd, epfd, connections);
            return;
        }
    }
}

void handle_write(int client_socketfd, int epfd, std::unordered_map<int, Client>& connections) {
    auto it = connections.find(client_socketfd);
    if (it == connections.end()) {
        return; // 连接已经关闭了
    }

    flush(client_socketfd, epfd, connections); // 发送 connections 里面的 outbuf
}

int main() {
    // 创建一个 epoll 登记表（指的是当前有哪些fd进入了epoll），以及一个就绪链表（指的是当前有哪些fd有事）
    // 在登记的时候可以设置 ev.events 字段来告诉内核这个fd有什么事情的时候就通知我，即告诉内核fd有什么事情将其挂到就绪链表
    // ev.events 是一个掩位码
    // ev.events 含有 EPOLLIN 表示这个fd属于可读状态时通知我
    // ev.events 含有 EPOLLOUT 表示这个fd属于可写状态时通知我
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        std::cerr << "create epoll error: " << std::error_code(errno, std::generic_category()).message() << std::endl;
        return 1;
    }
    // socket()
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {
        std::cerr << "socket error: " << std::error_code(errno, std::generic_category()).message() << std::endl;
        return 1;
    }

    // bind()
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080); // 转大端
    addr.sin_addr.s_addr = INADDR_ANY;
    int bind_result = bind(socketfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (bind_result == -1) {
        std::cerr << "bind error: " << std::error_code(errno, std::generic_category()).message() << std::endl;
        return 1;
    }

    // listen()
    int queue_size = 128;
    int listen_result = listen(socketfd, queue_size);
    if (listen_result == -1) {
        std::cerr << "listen error: " << std::error_code(errno, std::generic_category()).message() << std::endl;
        return 1;
    } else {
        std::cout << "listening on port 8080..." << std::endl;
    }

    // 设置 fd 为非阻塞
    fcntl(socketfd, F_SETFL, fcntl(socketfd, F_GETFL, 0) | O_NONBLOCK);

    // 启动服务器监听后，将这个 listen fd 增加到 epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = socketfd; // 记录 listen fd
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, socketfd, &ev) == -1) {
        std::cerr << "epoll add listen fd error: " << std::error_code(errno, std::generic_category()).message() << std::endl;
        return 1;
    }

    // 维护一个kv存储
    Store store;

    // 维护一个连接队列
    std::unordered_map<int, Client> connections;

    struct epoll_event events[64]; // 最多服务 64 个连接
    while(1) {
        int event_num = epoll_wait(epfd, events, 64, -1); // epoll wait 一直等待
        if (event_num == -1) {
            std::cerr << "epoll wait error: " << std::error_code(errno, std::generic_category()).message() << std::endl;
            continue; // 继续下一轮
        }

        for (int i=0; i<event_num; ++i) {
            int fd = events[i].data.fd;
            if (fd == socketfd) { // listenfd 响了，说明有新连接
                // accept() 阻塞
                struct sockaddr_in client_addr; // 客户端地址
                socklen_t client_addr_len = sizeof(client_addr);
                int client_socketfd = accept(socketfd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_len);
                if (client_socketfd == -1) {
                    std::cerr << "accept error: " << std::error_code(errno, std::generic_category()).message() << std::endl;
                    continue; // 继续等待下一个连接
                }

                // 新连接设成非阻塞
                fcntl(client_socketfd, F_SETFL, fcntl(client_socketfd, F_GETFL, 0) | O_NONBLOCK);

                char ip_buf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
                std::string peer = std::string(ip_buf) + ":" + std::to_string(ntohs(client_addr.sin_port));

                std::string querybuf; // 与客户端生命周期一致
                querybuf.reserve(4096); // 减少二次分配

                connections.insert({client_socketfd, Client(querybuf, peer)});

                // epoll 登记新的客户端连接 fd
                struct epoll_event ev;
                ev.events = EPOLLIN; // 告诉内核，以后这个fd可读的时候告诉我一声
                ev.data.fd = client_socketfd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_socketfd, &ev) == -1) { // 失败继续下一个连接
                    std::cerr << "epoll add client fd error: " << std::error_code(errno, std::generic_category()).message() << std::endl;
                    close(client_socketfd);
                    connections.erase(client_socketfd);
                    continue;
                }
            } else { // 客户端的 fd 有事了：从fd接收数据或者向fd发送数据，也可能同时进行
                if (events[i].events & EPOLLIN) { // 从fd接收数据
                    handle_read(fd, store, epfd, connections);
                }
                if (events[i].events & EPOLLOUT) { // 向fd发送数据
                    handle_write(fd, epfd, connections);
                }
            }
        }
    }

    // 关闭服务器套接字
    close(socketfd);

    return 0;
}