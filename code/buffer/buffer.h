#ifndef BUFFER_H
#define BUFFER_H
#include <assert.h>
#include <sys/uio.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <vector>
class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t ReadableBytes() const;     // 可读空间大小
    size_t WritableBytes() const;     // 可写空间大小
    size_t PrependableBytes() const;  // 已读空间大小

    const char *Peek() const;  // 可读区域起始地址
    char *BeginWrite();        // 可写区域的起始地址
    const char *BeginWriteConst() const;

    void EnsureWriteable(size_t len);  // 确保可写
    void HasWritten(size_t len);       // 已经写了,write指针向后移len

    // Retrieve = “把已经处理过的数据丢弃掉s”
    void Retrieve(size_t len);  // 已读len字节
    void RetrieveUntil(const char *end);// 直接读到end(read指针直接移到end字节)
    void RetrieveAll();              // 已读所有.read write清零
    std::string RetrieveAllToStr();  // 取出所有的可读字节,然后清零

    void Append(const char *str, size_t len);//把 str 开始的 len 个字节复制到 Buffer 的可写区域。
    void Append(const std::string &str);
    void Append(const void *data, size_t len);
    void Append(const Buffer &buff);

    ssize_t WriteFd(int fd, int* saveErrno);//把从peek开始的readsize大小的数据写入fd
    ssize_t ReadFd(int fd, int* saveErrno);//从fd里读取数据


private:
    char *BeginPtr_();
    const char *BeginPtr_() const;
    void MakeSpace_(size_t len);

    std::vector<char> buffer_;
    std::atomic<std::size_t> readPos_;
    std::atomic<std::size_t> writePos_;
};

#endif