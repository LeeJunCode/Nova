# socket

## 创建socket

    int socket(int domain, int type, int protocol) // 成功返回一个 socket 句柄，失败则返回 -1

1. domain 通讯的协议族

    ```
    PF_INET     // IPv4 一般都填这个
    PF_INET6    // IPv6
    PF_LOCAL    // 本地通信
    PF_PACKET   // 内核
    PE_IPX
    ```
2. type 数据传输的类型
   
    ```
    SOCK_STREAM     // 面向连接（顺序、数据都不会丢失，双向通道）
    SOCK_DGREAM     // 无连接
    ```
3. protocol 最终使用的协议
   
    SOCK_STREAM 对应 IPPROTO_TCP
    SOCK_DGREAM 对应 IPPROTO_UDP
    本参数可以填 0 ，编译器会自动识别
    ```
    socket(PF_INET, SOCK_STREAM, IPPROTO_TCP) // 创建 TCP 的 socket
    socket(PF_INET, SOCK_DGREAM, IPPROTO_UDP) // 创建 UDp 的 socket

    ```

## 大端序 小端序

内存地址：地址小的即为低位，地址大的即为高位
例如内存地址从 0X00000001 开始存储数据，那么从低位到高位依次为：
    
    0X00000001
    0X00000002
    0X00000003
    0X00000004

大端序：低位字节放在高位，高位字节放在低位
小段序：低位字节放在低位，高位字节放在高位

例如存储 0X12345678，对于要存储的数据来说，其中 78 就是低位， 12 就是高位

大端序存储结构

    0X00000001  0X12
    0X00000002  0X34
    0X00000003  0X56
    0X00000004  0X78

小端序存储结构

    0X00000001  0X78
    0X00000002  0X56
    0X00000003  0X34
    0X00000004  0X12

intel 一般为小端存储
但网络传输一般统一约定采用网络字节序即大端序

字节序列转换函数

    uint16_t htons(unit16_t hostshort) // 16 位主机转化为 16 位网络字节序
    uint32_t htonl(unit32_t hostlong)
    uint16_t ntohs(unit16_t netshort) // 16 位网络字节序转换为 16 位主机序列
    uint32_t ntohl(unit32_t netlong)

h：host
to：转换
n：network
s：short
l：long

## 网络通讯简单流程

Server

    socket()        // 创建 socket
    bind()          // 指定服务端用于通信的 ip 地址和 port
    listen()        // 监听
    accept()        // 接受客户端的连接
    recv()/send()   // 接收/发送数据
    close()         // 关闭 socket

Client

    socket()        // 创建 socket
    connect()       // 想服务端发起连接请求
    send()/recv()   // 发送/接收数据
    close()         // 关闭 socket

## socket 与 内核
程序编程使用的函数的交互对象是内核，而不是网络，也不是客户端
一个 socket 是一个文件描述符，对于一个程序，它创建的 socket 在内核中会创建一张 fd 表

    fd    内核对象
    1     socket对象（服务器监听用）
    2     socket对象（一个客户端连接用）

socket、bind、listen、accept、resv、send、close 等函数的操作对象都是那个文件描述符，而具体的操作都是由内核来完成的。函数所执行的读写操作对象也是内核缓冲区，resv是从内核缓冲区中去读数据，send是将数据写到内核缓冲区，数据的接收、发送、tcp连接都是由内核完成。

Server

    // 需要说明的是：不管这个程序是否存在，内核是一直在收包的，也就是哪怕没有编写服务端程序，只要有服务端地址，客户端就可以给服务端发包，在没有 socket 能够进行处理的时候，内核根据 tcp 协议直接回复 RST，这个过程与程序完全无关。

    // 创建一个 fd，即在那张表中添加一个 socket 对象。
    int socket(int domain, int type, int protocol);

    // 为这个 socket 对象绑定服务端的地址和端口
    int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

    // 告诉内核 这个服务器 socket 开始接客了
    // 内核做：① socket 状态 = TCP_LISTEN; ② 创建两个队列(SYN 队列 + 全连接队列)
    // 在开启监听之后，内核就可以建立 tcp 三次握手连接了
    // 在 listen 代码之后，内核开始进行 tcp 三次握手，过程如下：
    // 客户端发送 SYN     "SYN=1, Seq=x" 给服务端；                （第一次连接）
    // 服务端回复 SYN-ACK "SYN=1, ACK=1, Seq=y, Ack=x+1" 给客户端；（第二次连接，内核将其放入 SYN 半连接队列）
    // 客户端回复 ACK     "ACK=1, Seq=x+1, Ack=y+1" 给服务端；     （第三次连接，至此，tcp连接成功建立，内核将其放入 全连接队列）
    int listen(int sockfd, int backlog);

    // 正式开始循环接客了
    // 这里的循环是指循环从全连接队列中去拿一个已经建立连接的客户端对象
    // 如果全连接队列里有东西，就执行程序，将这个客户端交给一个线程去处理，继续下一个循环
    // 如果全连接队列空的，那就阻塞在 accept，直到有客户端建立连接
    while (1) {
        // 从内核给的全连接队列中去获取客户端地址
        // 阻塞：如果全连接队列里面没东西，那就阻塞在这里，线程转为阻塞态，程序暂停
        // 如果全连接队列里面有东西，就返回一个新的 fd 来指向内核中的这个客户端对象，fd 表里添加一个 fd 来指向内核对象
        int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

        // 可以开一个线程去对这个客户端进行服务，也可以直接在这一次循环中进行服务，如果不开线程，那么一次只能服务一个客户端
        thread t ：{
            // 开始循环接收数据并处理
            while (1) {
                ssize_t recv(int sockfd, void *buf, size_t len, int flags);

                /*
                处理过程
                */

                // 循环发送，确保所有数据发送完毕
                while (1) {
                    ssize_t send(int sockfd, const void *buf, size_t len, int flags);
                }
            }
            close(); // 关闭 fd 表中指向客户端的那个 fd，在内核中会将这个客户端对象的引用计数-1，减为0之后进入四次挥手，断开连接
        }
    }

    close(); // 关闭 fd 表中指向服务端的那个fd