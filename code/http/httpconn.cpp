/*     // 多行注释开始
 * @Author       : mark     // 作者标识
 * @Date         : 2020-06-15     // 文件创建日期
 * @copyleft Apache 2.0     // Apache 2.0 开源许可
 */      // 多行注释结束

#include "httpconn.h"     // 包含 HTTP 连接类的头文件定义
using namespace std;      // 使用标准命名空间，简化 std:: 前缀

const char* HttpConn::srcDir;             // 定义静态成员：静态资源根目录路径
std::atomic<int> HttpConn::userCount;     // 定义静态成员：当前连接数（原子类型保证线程安全）
bool HttpConn::isET;                      // 定义静态成员：是否使用边缘触发（ET）模式

HttpConn::HttpConn() {     // 默认构造函数定义
    fd_ = -1;               // 初始化文件描述符为 -1（表示无效）
    addr_ = { 0 };          // 初始化地址结构体为全 0
    isClose_ = true;        // 初始化关闭标志为 true
};     // 构造函数结束

HttpConn::~HttpConn() {     // 析构函数定义
    Close();                // 调用 Close 清理资源
};     // 析构函数结束

void HttpConn::init(int fd, const sockaddr_in& addr) {     // 初始化连接函数，参数：fd（文件描述符）、addr（客户端地址）
    assert(fd > 0);                 // 断言：确保 fd 有效（大于 0）
    userCount++;                    // 连接计数器加 1
    addr_ = addr;                   // 保存客户端地址信息
    fd_ = fd;                       // 保存文件描述符
    writeBuff_.RetrieveAll();       // 清空写缓冲区的所有数据
    readBuff_.RetrieveAll();        // 清空读缓冲区的所有数据
    isClose_ = false;               // 设置连接状态为打开
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);     // 记录客户端连接信息日志
}     // init 函数结束

void HttpConn::Close() {     // 关闭连接函数
    response_.UnmapFile();          // 解除响应对象中的文件内存映射
    if(isClose_ == false){          // 如果连接还未关闭
        isClose_ = true;            // 设置关闭标志为 true
        userCount--;                // 连接计数器减 1
        close(fd_);                 // 关闭 socket 文件描述符
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);     // 记录客户端断开连接日志
    }
}     // Close 函数结束

int HttpConn::GetFd() const {     // 获取文件描述符的 const 成员函数
    return fd_;                   // 返回 socket 文件描述符
};     // GetFd 函数结束

struct sockaddr_in HttpConn::GetAddr() const {     // 获取客户端地址的 const 成员函数
    return addr_;                                   // 返回客户端地址结构体
}     // GetAddr 函数结束

const char* HttpConn::GetIP() const {         // 获取客户端 IP 地址的 const 成员函数
    return inet_ntoa(addr_.sin_addr);     // 将网络字节序 IP 转换为点分十进制字符串并返回
}     // GetIP 函数结束

int HttpConn::GetPort() const {     // 获取客户端端口号的 const 成员函数
    return addr_.sin_port;      // 返回网络字节序的端口号
}     // GetPort 函数结束

ssize_t HttpConn::read(int* saveErrno) {           // 从 socket 读取数据函数，参数：saveErrno（保存错误码的指针）
    ssize_t len = -1;                              // 初始化读取长度为 -1
    do {                                           // 开始循环读取
        len = readBuff_.ReadFd(fd_, saveErrno);    // 调用缓冲区的 ReadFd 方法从 fd 读取数据
        if (len <= 0) {                            // 如果读取失败或连接关闭（返回 <=0）
            break;                                 // 跳出循环
        }
    } while (isET);                                // 如果是 ET 模式则继续循环读取（直到无数据）
    return len;                                    // 返回最后一次读取的字节数
}     // read 函数结束

ssize_t HttpConn::write(int* saveErrno) {                                         // 向 socket 写入数据函数，参数：saveErrno（保存错误码的指针）
    ssize_t len = -1;                                                              // 初始化写入长度为 -1
    do {                                                                           // 开始循环写入
        len = writev(fd_, iov_, iovCnt_);                                          // 使用 writev 聚合写入多个缓冲区
        if(len <= 0) {                                                             // 如果写入失败（返回 <=0）
            *saveErrno = errno;                                                    // 保存错误码到指针指向的变量
            break;                                                                 // 跳出循环
        }
        
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; }                    // 如果两个缓冲区都已写完，跳出循环
        
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {                     // 如果写入字节数超过第一个缓冲区长度
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);     // 调整第二个缓冲区起始地址（向后移动）
            iov_[1].iov_len -= (len - iov_[0].iov_len);                           // 减少第二个缓冲区剩余长度
            if(iov_[0].iov_len) {                                                  // 如果第一个缓冲区长度不为 0
                writeBuff_.RetrieveAll();                                          // 清空写缓冲区
                iov_[0].iov_len = 0;                                               // 将第一个缓冲区长度设为 0
            }
        }
        else {                                                                     // 如果只写入了第一个缓冲区的部分数据
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;                  // 调整第一个缓冲区起始地址（向后移动）
            iov_[0].iov_len -= len;                                                // 减少第一个缓冲区剩余长度
            writeBuff_.Retrieve(len);                                              // 从写缓冲区中移除已写入的字节
        }
    } while(isET || ToWriteBytes() > 10240);                                       // 如果是 ET 模式或剩余数据大于 10KB 则继续循环
    return len;                                                                    // 返回最后一次写入的字节数
}     // write 函数结束

bool HttpConn::process() {                                                    // 处理 HTTP 请求的主函数
    request_.Init();                                                          // 初始化请求对象（重置状态）
    
    if(readBuff_.ReadableBytes() <= 0) {                                      // 如果读缓冲区没有可读数据
        return false;                                                         // 返回 false 表示处理失败
    }
    else if(request_.parse(readBuff_)) {                                      // 尝试解析 HTTP 请求
        LOG_DEBUG("%s", request_.path().c_str());                             // 记录请求路径到调试日志
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200); // 初始化响应对象（状态码 200）
    } else {                                                                  // 如果解析失败
        response_.Init(srcDir, request_.path(), false, 400);                  // 初始化响应对象（状态码 400）
    }

    response_.MakeResponse(writeBuff_);                                       // 生成 HTTP 响应并写入写缓冲区
    
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());                 // 设置第一个 iovec 的起始地址（响应头）
    iov_[0].iov_len = writeBuff_.ReadableBytes();                            // 设置第一个 iovec 的长度
    iovCnt_ = 1;                                                              // 设置 iovec 数组元素个数为 1

    if(response_.FileLen() > 0  && response_.File()) {                        // 如果响应包含文件且文件长度大于 0
        iov_[1].iov_base = response_.File();                                  // 设置第二个 iovec 的起始地址（文件内容）
        iov_[1].iov_len = response_.FileLen();                                // 设置第二个 iovec 的长度（文件大小）
        iovCnt_ = 2;                                                          // 设置 iovec 数组元素个数为 2
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());     // 记录文件大小、缓冲区数量、待写入总字节数
    return true;                                                              // 返回 true 表示处理成功
}     // process 函数结束
