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