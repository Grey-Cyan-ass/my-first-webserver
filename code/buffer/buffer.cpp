#include "buffer.h" // 引入 Buffer 类的头文件

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {} // 构造函数：初始化缓冲区大小以及读写位置

size_t Buffer::ReadableBytes() const { // 返回当前可读字节数
    return writePos_ - readPos_; // 可读区间为 [readPos_, writePos_)
}
size_t Buffer::WritableBytes() const { // 返回当前可写字节数
    return buffer_.size() - writePos_; // 可写区间为 [writePos_, buffer_.size())
}
size_t Buffer::PrependableBytes() const { // 返回已读区域大小（可前置写入的空间）
    return readPos_; // 已经读取过的数据之前的空间
}


const char* Buffer::Peek() const { // 获取当前读指针（只读）
    return BeginPtr_() + readPos_; // 指向可读区域的起始位置
}

void Buffer::Retrieve(size_t len) { // 从缓冲区取出 len 字节（前移读指针）
    assert(len <= ReadableBytes()); // 断言：取出长度不能超过可读字节数
    readPos_ += len; // 前移读指针
}

void Buffer::RetrieveUntil(const char* end) { // 读取直到 end 指针位置（不包含 end）
    assert(Peek() <= end ); // 断言：end 位置应当在可读范围内
    Retrieve(end - Peek()); // 计算应当取出的长度并前移读指针
}

void Buffer::RetrieveAll() { // 清空缓冲区内容
    bzero(&buffer_[0], buffer_.size()); // 将底层存储清零
    readPos_ = 0; // 重置读指针
    writePos_ = 0; // 重置写指针
}

std::string Buffer::RetrieveAllToStr() { // 读取全部数据并返回为字符串
    std::string str(Peek(), ReadableBytes()); // 基于可读区域构造字符串
    RetrieveAll(); // 清空缓冲区
    return str; // 返回字符串
}

const char* Buffer::BeginWriteConst() const { // 获取当前写指针（只读）
    return BeginPtr_() + writePos_; // 指向可写区域的起始位置
}

char* Buffer::BeginWrite() { // 获取当前写指针（可写）
    return BeginPtr_() + writePos_; // 指向可写区域的起始位置
}

void Buffer::HasWritten(size_t len) { // 标记已经写入了 len 字节（后移写指针）
    writePos_ += len; // 后移写指针
} 

void Buffer::Append(const std::string& str) { // 追加字符串数据到缓冲区
    Append(str.data(), str.length()); // 委托到通用追加函数
}

void Buffer::Append(const void* data, size_t len) { // 追加任意二进制数据
    assert(data); // 断言：数据指针非空
    Append(static_cast<const char*>(data), len); // 转为 char* 并复用实现
}

void Buffer::Append(const char* str, size_t len) { // 追加字符数组数据
    assert(str); // 断言：指针非空
    EnsureWriteable(len); // 确保有足够可写空间，不足则扩容或整理
    std::copy(str, str + len, BeginWrite()); // 将数据拷贝至写位置
    HasWritten(len); // 更新写指针
}

void Buffer::Append(const Buffer& buff) { // 从另一个 Buffer 追加可读数据
    Append(buff.Peek(), buff.ReadableBytes()); // 复用追加函数
}

void Buffer::EnsureWriteable(size_t len) { // 确保可写空间 >= len
    if(WritableBytes() < len) { // 如果空间不足
        MakeSpace_(len); // 进行扩容或数据前移
    }
    assert(WritableBytes() >= len); // 断言：现在空间必须足够
}

ssize_t Buffer::ReadFd(int fd, int* saveErrno) { // 从文件描述符读取数据到缓冲区
    char buff[65535]; // 栈上临时缓冲用于分散读
    struct iovec iov[2]; // 两个 iovec 执行分散读
    const size_t writable = WritableBytes(); // 记录当前主缓冲可写空间
    /* 分散读， 保证数据全部读完 */
    iov[0].iov_base = BeginPtr_() + writePos_; // 第一段指向主缓冲的可写区
    iov[0].iov_len = writable; // 第一段长度为可写空间
    iov[1].iov_base = buff; // 第二段指向临时缓冲
    iov[1].iov_len = sizeof(buff); // 第二段长度为临时缓冲大小

    const ssize_t len = readv(fd, iov, 2); // 分散读：最多写入两段缓冲
    if(len < 0) { // 读取失败
        *saveErrno = errno; // 保存错误码
    }
    else if(static_cast<size_t>(len) <= writable) { // 数据全部写入到主缓冲
        writePos_ += len; // 仅后移主缓冲写指针
    }
    else { // 数据超过主缓冲可写空间，一部分落入临时缓冲
        writePos_ = buffer_.size(); // 主缓冲写满
        Append(buff, len - writable); // 将临时缓冲中的剩余数据追加到主缓冲
    }
    return len; // 返回读取的总字节数
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) { // 将缓冲区数据写入文件描述符
    size_t readSize = ReadableBytes(); // 计算可写出的字节数（可读区大小）
    ssize_t len = write(fd, Peek(), readSize); // 一次性写出可读区域
    if(len < 0) { // 写失败
        *saveErrno = errno; // 保存错误码
        return len; // 返回失败值
    } 
    readPos_ += len; // 写成功则前移读指针，表示这些数据已被消费
    return len; // 返回写出的字节数
}

char* Buffer::BeginPtr_() { // 获取底层存储起始地址（可写）
    return &*buffer_.begin(); // 通过迭代器取地址，兼容空向量的标准写法
}

const char* Buffer::BeginPtr_() const { // 获取底层存储起始地址（只读）
    return &*buffer_.begin(); // 同上，const 版本
}

void Buffer::MakeSpace_(size_t len) { // 在空间不足时扩容或整理空间
    if(WritableBytes() + PrependableBytes() < len) { // 即使前移也不够，则需要扩容
        buffer_.resize(writePos_ + len + 1); // 扩容：至少容纳当前写位置 + 需要长度 + 1
    } 
    else { // 通过前移未读数据来腾出空间
        size_t readable = ReadableBytes(); // 未读数据长度
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_()); // 将未读数据前移到起始处
        readPos_ = 0; // 重置读指针到起始
        writePos_ = readPos_ + readable; // 写指针定位到未读数据之后
        assert(readable == ReadableBytes()); // 校验数据量未改变
    }
}
