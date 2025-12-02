/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft Apache 2.0
 */ 

// 包含日志类的头文件
#include "log.h"

using namespace std;

// 日志类的构造函数：初始化所有成员变量
Log::Log() {
    lineCount_ = 0;      // 日志行数计数器置零
    isAsync_ = false;    // 默认为同步日志模式
    writeThread_ = nullptr;  // 写入线程指针初始化为空
    deque_ = nullptr;    // 阻塞队列指针初始化为空
    toDay_ = 0;         // 当前日期初始化为0
    fp_ = nullptr;      // 文件指针初始化为空
}

// 日志类的析构函数：清理资源
Log::~Log() {
    // 如果异步写线程存在且可以join
    if(writeThread_ && writeThread_->joinable()) {
        // 确保队列中的所有日志都被写入
        while(!deque_->empty()) {
            deque_->flush();
        };
        deque_->Close();        // 关闭队列
        writeThread_->join();   // 等待写线程结束
    }
    // 如果文件指针存在，清理文件相关资源
    if(fp_) {
        lock_guard<mutex> locker(mtx_);  // 加锁保护
        flush();                         // 刷新缓冲区
        fclose(fp_);                     // 关闭文件
    }
}

// 获取日志级别
int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);  // 加锁保护
    return level_;
}

// 设置日志级别
void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);  // 加锁保护
    level_ = level;
}

// 初始化日志系统
// level: 日志级别
// path: 日志文件路径
// suffix: 日志文件后缀
// maxQueueSize: 异步日志队列大小，大于0则开启异步日志
void Log::init(int level = 1, const char* path, const char* suffix,
    int maxQueueSize) {
    isOpen_ = true;      // 打开日志功能
    level_ = level;      // 设置日志级别
    
    // 根据队列大小决定是否使用异步日志
    if(maxQueueSize > 0) {
        isAsync_ = true;  // 开启异步
        if(!deque_) {    // 如果队列未创建
            // 创建阻塞队列
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = move(newDeque);
            
            // 创建异步写线程
            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            writeThread_ = move(NewThread);
        }
    } else {
        isAsync_ = false;  // 使用同步日志
    }

    lineCount_ = 0;      // 重置行数计数

    // 获取当前时间
    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    
    // 设置路径和后缀
    path_ = path;
    suffix_ = suffix;
    
    // 生成日志文件名：路径/年_月_日后缀
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    toDay_ = t.tm_mday;  // 记录当前日期

    {
        // 加锁保护，处理文件操作
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();  // 清空缓冲区
        if(fp_) {            // 如果文件已经打开
            flush();         // 刷新缓冲区
            fclose(fp_);     // 关闭旧文件
        }   

        // 以追加模式打开新文件
        fp_ = fopen(fileName, "a");
        if(fp_ == nullptr) {
            mkdir(path_, 0777);    // 如果目录不存在，创建目录
            fp_ = fopen(fileName, "a");  // 重试打开文件
        } 
        assert(fp_ != nullptr);    // 确保文件打开成功
    }
}

// 写入日志的主要函数
void Log::write(int level, const char *format, ...) {
    // 获取当前时间，精确到微秒
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    // 检查是否需要新建日志文件
    // 条件：1. 日期变化 2. 当前日志行数达到最大限制
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();  // 临时解锁以减少锁的持有时间
        
        // 准备新文件名
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        // 生成日期字符串
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        // 如果是新的一天
        if (toDay_ != t.tm_mday)
        {
            // 创建新的日志文件
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;    // 更新当前日期
            lineCount_ = 0;        // 重置行数计数器
        }
        else {
            // 如果是同一天但达到行数限制，创建新文件，文件名加上序号
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        // 重新加锁，切换到新文件
        locker.lock();
        flush();                    // 刷新旧文件
        fclose(fp_);               // 关闭旧文件
        fp_ = fopen(newFile, "a"); // 打开新文件
        assert(fp_ != nullptr);    // 确保文件打开成功
    }

    {
        // 加锁保护，写入日志内容
        unique_lock<mutex> locker(mtx_);
        lineCount_++;  // 行数增加
        
        // 写入时间戳
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                    
        buff_.HasWritten(n);        // 更新缓冲区写入位置
        AppendLogLevelTitle_(level); // 添加日志级别标题

        // 处理变参数组，写入日志内容
        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);       // 更新写入长度
        buff_.Append("\n\0", 2);   // 添加换行和结束符

        // 判断是使用异步还是同步写入
        if(isAsync_ && deque_ && !deque_->full()) {
            // 异步模式：将日志内容推送到队列
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {
            // 同步模式：直接写入文件
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();  // 清空缓冲区
    }
}

// 添加日志级别标题
void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);  // 调试级别
        break;
    case 1:
        buff_.Append("[info] : ", 9);  // 信息级别
        break;
    case 2:
        buff_.Append("[warn] : ", 9);  // 警告级别
        break;
    case 3:
        buff_.Append("[error]: ", 9);  // 错误级别
        break;
    default:
        buff_.Append("[info] : ", 9);  // 默认为信息级别
        break;
    }
}

// 刷新日志缓冲区
void Log::flush() {
    if(isAsync_) { 
        deque_->flush();  // 异步模式：通知写线程处理队列中的日志
    }
    fflush(fp_);         // 刷新文件缓冲区
}

// 异步写入线程的执行函数
void Log::AsyncWrite_() {
    string str = "";
    // 持续从队列中获取日志消息并写入文件
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);  // 加锁保护文件写入
        fputs(str.c_str(), fp_);         // 写入文件
    }
}

// 获取日志单例实例
Log* Log::Instance() {
    static Log inst;    // 静态局部变量实现单例模式
    return &inst;      //创建一个inst对象，不会改变，永远返回的是同一个地址
}

// 异步日志线程的入口函数
void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();  // 启动异步写入处理
}