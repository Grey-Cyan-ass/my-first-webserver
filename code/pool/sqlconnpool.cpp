/*
 * @Author       : mark                         // 作者
 * @Date         : 2020-06-17                   // 创建日期
 * @copyleft Apache 2.0                         // 许可证
 */ 

#include "sqlconnpool.h"                         // 引入连接池头文件
using namespace std;                              // 使用标准命名空间

SqlConnPool::SqlConnPool() {                      // 构造函数
    useCount_ = 0;                                // 当前使用中的连接数（未在此处主动维护）
    freeCount_ = 0;                               // 当前空闲连接数（未在此处主动维护）
}

SqlConnPool* SqlConnPool::Instance() {            // 单例获取接口
    static SqlConnPool connPool;                  // 静态局部单例对象（线程安全初始化）  这个变量只被创建一次
    return &connPool;                             // 返回单例指针
}

void SqlConnPool::Init(const char* host, int port, // 初始化连接池：建立若干 MySQL 连接
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {
    assert(connSize > 0);                         // 断言连接数为正
    for (int i = 0; i < connSize; i++) {          // 循环创建 connSize 个连接
        MYSQL *sql = nullptr;                      // 声明连接指针
        sql = mysql_init(sql);                     // 初始化 MYSQL 句柄
        if (!sql) {                                // 判空：初始化失败
            LOG_ERROR("MySql init error!");       // 打印错误日志
            assert(sql);                           // 触发断言方便定位
        }
        sql = mysql_real_connect(sql, host,        // 建立真实连接
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql) {                                // 连接失败
            LOG_ERROR("MySql Connect error!");    // 打印错误日志（未中止）
        }
        connQue_.push(sql);                        // 成功或失败的句柄都入队（失败时为 nullptr）
    }
    MAX_CONN_ = connSize;                          // 记录最大连接数
    sem_init(&semId_, 0, MAX_CONN_);               // 初始化信号量，初值为可用连接数
}

MYSQL* SqlConnPool::GetConn() {                    // 从池中获取一个连接
    MYSQL *sql = nullptr;                          // 结果指针
    if(connQue_.empty()){                          // 若当前队列为空
        LOG_WARN("SqlConnPool busy!");            // 打印告警：连接已耗尽
        return nullptr;                            // 直接返回空指针（未阻塞等待）
    }
    sem_wait(&semId_);                             // 消耗一个信号量名额（可能阻塞）
    {
        lock_guard<mutex> locker(mtx_);            // 加锁保护队列
        sql = connQue_.front();                    // 取队头连接
        connQue_.pop();                            // 出队
    }
    return sql;                                    // 返回连接指针
}
 
void SqlConnPool::FreeConn(MYSQL* sql) {           // 归还一个连接到池中
    assert(sql);                                   // 断言：归还的连接不应为 nullptr
    lock_guard<mutex> locker(mtx_);                // 加锁保护队列
    connQue_.push(sql);                            // 放回队列
    sem_post(&semId_);                             // 释放一个信号量名额（唤醒等待者）
}

void SqlConnPool::ClosePool() {                    // 关闭连接池并释放资源
    lock_guard<mutex> locker(mtx_);                // 加锁保护
    while(!connQue_.empty()) {                     // 遍历队列
        auto item = connQue_.front();              // 取队头
        connQue_.pop();                            // 出队
        mysql_close(item);                         // 关闭单个连接
    }
    mysql_library_end();                           // 关闭 MySQL 客户端库
}

int SqlConnPool::GetFreeConnCount() {              // 获取空闲连接数量
    lock_guard<mutex> locker(mtx_);                // 加锁保护
    return connQue_.size();                        // 返回队列大小
}

SqlConnPool::~SqlConnPool() {                      // 析构函数
    ClosePool();                                   // 析构时关闭连接池
}
