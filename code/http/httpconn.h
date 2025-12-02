/* // 多行注释开始
 * @Author       : mark // 作者信息
 * @Date         : 2020-06-15 // 创建日期
 * @copyleft Apache 2.0 // 许可证信息
 */  // 多行注释结束

#ifndef HTTP_CONN_H // 防止头文件重复包含的宏定义开始
#define HTTP_CONN_H // 定义头文件保护宏

#include <sys/types.h> // 包含系统类型定义（如 ssize_t）
#include <sys/uio.h> // 包含向量 I/O 函数（writev/readv）
#include <arpa/inet.h> // 包含网络地址转换函数（inet_ntoa）
#include <stdlib.h> // 包含标准库函数
#include <errno.h> // 包含错误码定义

#include "../log/log.h" // 包含日志系统
#include "../pool/sqlconnRAII.h" // 包含 SQL 连接池 RAII 封装
#include "../buffer/buffer.h" // 包含缓冲区类
#include "httprequest.h" // 包含 HTTP 请求解析类
#include "httpresponse.h" // 包含 HTTP 响应生成类

class HttpConn { // 定义 HTTP 连接类
public: // 公有成员
    HttpConn(); // 默认构造函数声明

    ~HttpConn(); // 析构函数声明

    void init(int sockFd, const sockaddr_in& addr); // 初始化连接，参数：socket 文件描述符、客户端地址

    ssize_t read(int* saveErrno); // 从 socket 读取数据，参数：保存错误码的指针

    ssize_t write(int* saveErrno); // 向 socket 写入数据，参数：保存错误码的指针

    void Close(); // 关闭连接并清理资源

    int GetFd() const; // 获取 socket 文件描述符

    int GetPort() const; // 获取客户端端口号

    const char* GetIP() const; // 获取客户端 IP 地址字符串
    
    sockaddr_in GetAddr() const; // 获取客户端地址结构体
    
    bool process(); // 处理 HTTP 请求并生成响应

    int ToWriteBytes() { // 计算待写入的总字节数（内联函数）
        return iov_[0].iov_len + iov_[1].iov_len; // 返回两个 iovec 缓冲区长度之和
    }

    bool IsKeepAlive() const { // 判断是否为长连接（内联函数）
        return request_.IsKeepAlive(); // 委托给 request_ 对象判断
    }

    static bool isET; // 静态成员：是否使用边缘触发（ET）模式
    static const char* srcDir; // 静态成员：静态资源根目录路径
    static std::atomic<int> userCount; // 静态成员：当前连接数（原子类型保证线程安全）
    
private: // 私有成员
    int fd_; // socket 文件描述符
    struct  sockaddr_in addr_; // 客户端地址信息

    bool isClose_; // 连接是否已关闭的标志
    
    int iovCnt_; // iovec 数组的有效元素数量（1 或 2）
    struct iovec iov_[2]; // writev 使用的向量缓冲区数组（0：响应头，1：文件）
    
    Buffer readBuff_; // 读缓冲区对象
    Buffer writeBuff_; // 写缓冲区对象

    HttpRequest request_; // HTTP 请求解析对象
    HttpResponse response_; // HTTP 响应生成对象
}; // 类定义结束


#endif //HTTP_CONN_H // 头文件保护宏结束