### 宏

### inline

### static 变量 懒加载

### 一切皆文件
网络连接也是文件，tcp连接就是一个文件描述符（fd），支持 read、write、close
server 标准流程

    // 1. socket()
    // int socket(int domain, int type, int protocol);
    // domain: 协议族，AF_INET（IPv4）、AF_INET6（IPv6）、AF_UNIX（本地通信）等
    // type: 套接字类型，SOCK_STREAM（面向连接的流式套接字，TCP）、SOCK_DGRAM（无连接的数据报套接字，UDP）等
    // protocol: 协议，通常为 0，表示使用默认协议TCP
    // 返回一个非负整数表示文件描述符
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);

    // 2. bind()
    // int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    // sockfd: 套接字文件描述符
    // addr: 指向套接字地址结构的指针，包含IP地址和端口号
    // addrlen: 套接字地址结构的长度
    // 返回 0 表示成功，-1 表示失败
    struct sockaddr_in addr; // 一个socket结构体，包含协议族、端口号、IP地址
    addr.sin_family = AF_INET; // IPV4
    addr.sin_port = htons(8080); // 端口号，htons() host to network short，将主机字节序转换（小端）为网络字节序（大端）
    addr.sin_addr.s_addr = INADDR_ANY; // IP地址，使用 INADDR_ANY
    int bind_result = bind(socketfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

    // 3. listen()
    // int listen(int sockfd, int backlog);
    // sockfd: 套接字文件描述符
    // backlog: 连接请求队列的最大长度
    // 返回 0 表示成功，-1 表示失败
    int listen_result = listen(socketfd, 5);

    // 外层循环，处理客户端连接
    while(1) {
        // 4. accept()
        // int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
        // sockfd: 套接字文件描述符
        // addr: 指向套接字地址结构的指针，用于存储客户端的地址信息
        // addrlen: 指向套接字地址结构长度的指针，传入时表示缓冲区大小，返回时表示实际地址长度
        // 返回一个新的套接字文件描述符，用于与客户端通信
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socketfd = accept(socketfd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_len);
        if (client_socketfd == -1) {
            continue; // 继续等待下一个连接
        }

        // 内层循环，处理这一个客户端消息
        while (1) {
            // 5. recv()
            // ssize_t recv(int sockfd, void *buf, size_t len, int flags);
            // sockfd: 套接字文件描述符
            // buf: 接收数据的缓冲区
            // len: 缓冲区大小
            // flags: 标志位，通常为 0
            // 返回接收到的字节数，-1 表示失败，0 表示连接关闭
            char buffer[1024];
            ssize_t bytes_received = recv(client_socketfd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received == -1) {
                break; // 继续等待下一个连接
            } else if (bytes_received == 0) {
                break; // 继续等待下一个连接
            }

            buffer[bytes_received] = '\0'; // 确保字符串以 null 结尾

            // 6. send()
            // ssize_t send(int sockfd, const void *buf, size_t len, int flags);
            // sockfd: 套接字文件描述符
            // buf: 发送数据的缓冲区
            // len: 缓冲区大小
            // flags: 标志位，通常为 0
            // 返回发送的字节数，-1 表示失败
            const char* response = buffer; // 直接回显客户端发送的消息
            ssize_t bytes_sent = send(client_socketfd, response, bytes_received, 0);
        }

        // 关闭客户端套接字
        close(client_socketfd);
    }

    // 关闭服务器套接字
    close(socketfd);