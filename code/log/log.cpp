#include "log.h"

using namespace std;

Log* Log::Instance() {
    static Log inst;
    return &inst;
}

void Log::FlushLogThread() { Log::Instance()->AsyncWrite_(); }

void Log::init(int level = 1, const char* path, const char* suffix,
               int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    if (maxQueueSize > 0) {  // 日志系统异步模式下，deque_的最大容量
        // deque_的最大容量>0,说明是异步
        isAsync_ = true;  // 异步模式
        if (!deque_) {
            unique_ptr<BlockDeque<std::string>> newDeque(
                new BlockDeque<std::string>);  // 如果没有deque,就创建一个
            deque_ = move(newDeque);
            std::unique_ptr<std::thread> NewThread(
                new thread(FlushLogThread));  // 创建一个线程
            writeThread_ = move(NewThread);
        }
    } else {
        isAsync_ = false;
    }
    lineCount_ = 0;
    time_t timer = time(nullptr);
    struct tm* sysTime = localtime(&timer);
    struct tm t = *sysTime;
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName,
             LOG_NAME_LEN - 1,
             "%s/%04d_%02d_%02d%s",
             path_,
             t.tm_year + 1900,
             t.tm_mon + 1,
             t.tm_mday,
             suffix_);
    toDay_ = t.tm_mday;
    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();
        if (fp_) {
            flush();
            fclose(fp_);
        }

        fp_ = fopen(fileName, "a");
        if (fp_ == nullptr) {
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }
}

// 先判断是否轮转,然后异步写入deque_,同步直接写
void Log::write(int level, const char* format, ...) {
    // now 是一个 timeval 结构体，包含 tv_sec（秒）和 tv_usec（微秒）
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);  // 获取当前时间,写入now,nullptr表示不使用时区
    time_t tSec = now.tv_sec;     // 取出秒
    struct tm* sysTime = localtime(&tSec);  // 将tSec转换为本地时间
    struct tm t = *sysTime;
    va_list vaList;  // 遍历可变参数

    // 判断是否要轮转
    bool need_rotate =
        (toDay_ != t.tm_mday) || (lineCount_ && (lineCount_ % MAX_LINES == 0));
    if (need_rotate) {  // 确定要轮换,关闭旧日志,打开新日志
        char newFile[LOG_NAME_LEN];
        // 生成年月日,因为不是共享数据,无需加锁保护,先生成
        char tail[36] = {0};
        snprintf(tail,
                 36,
                 "%04d_%02d_%02d",
                 t.tm_year + 1900,
                 t.tm_mon + 1,
                 t.tm_mday);
        // 上锁
        unique_lock<mutex> locker(mtx_);
        // 再次确定是否需要轮转
        if ((toDay_ != t.tm_mday) ||
            (lineCount_ && (lineCount_ % MAX_LINES == 0))) {
            if (toDay_ != t.tm_mday) {
                snprintf(newFile,
                         LOG_NAME_LEN - 72,
                         "%s/%s%s",
                         path_,
                         tail,
                         suffix_);
                toDay_ = t.tm_mday;
                lineCount_ = 0;
            } else {
                snprintf(newFile,
                         LOG_NAME_LEN - 72,
                         "%s/%s-%d%s",
                         path_,
                         tail,
                         (lineCount_ / MAX_LINES),  // 表示第几个日志文件
                         suffix_);
            }
            flush();
            fclose(fp_);
            fp_ = fopen(newFile, "a");  // fp指向新文件
            assert(fp_ != nullptr);
        }
    }

    {  // 开始写入日志
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        // n返回的是实际写入的长度
        // 写入buff
        int n = snprintf(buff_.BeginWrite(),
                         128,
                         "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                         t.tm_year + 1900,
                         t.tm_mon + 1,
                         t.tm_mday,
                         t.tm_hour,
                         t.tm_min,
                         t.tm_sec,
                         now.tv_usec);

        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);

        // 初始化 va_list 变量，使其指向可变参数列表的第一个参数
        va_start(vaList, format);
        // 写入缓冲区
        int m = vsnprintf(
            buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);  // 清理
        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        // 异步模式,把日志写入log的缓冲区deque_
        if (isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {  // 同步模式,直接写入文件
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();
    }
}

void Log::flush() {
    if (isAsync_) {
        deque_->flush();  // 确保所有数据都写入到deque_
    }
    fflush(fp_);  // 清空缓冲区
}

int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}
void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

Log::~Log() {
    if (writeThread_ && writeThread_->joinable()) {
        while (!deque_->empty()) {
            deque_->flush();
        };
        deque_->Close();
        writeThread_->join();
    }

    if (fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

void Log::AppendLogLevelTitle_(int level) {
    switch (level) {
        case 0:
            buff_.Append("[debug]: ", 9);
            break;
        case 1:
            buff_.Append("[info] : ", 9);
            break;
        case 2:
            buff_.Append("[warn] : ", 9);
            break;
        case 3:
            buff_.Append("[error]: ", 9);
            break;
        default:
            buff_.Append("[info] : ", 9);
            break;
    }
}

void Log::AsyncWrite_() {  // 异步写入
    string str = "";
    while (deque_->pop(str)) {  // 从deque里取出值到str里面
        lock_guard<mutex> locker(mtx_);
        // 将c++风格的str字符串转变为c风格的,然后写入fp指向的文件
        fputs(str.c_str(), fp_);
    }
}
