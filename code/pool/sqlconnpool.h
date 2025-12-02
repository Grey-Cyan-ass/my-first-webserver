/*
 * @Author       : mark                 // 作者信息
 * @Date         : 2020-06-16           // 创建日期
 * @copyleft Apache 2.0                 // 许可协议
 */                                      // 文件头部注释结束
#ifndef SQLCONNPOOL_H                     // 头文件保护宏：若未定义则继续
#define SQLCONNPOOL_H                     // 定义头文件保护宏，防止重复包含

#include <mysql/mysql.h>                  // 引入 MySQL C API 的头文件（MYSQL 类型等）
#include <string>                         // 引入 std::string（可能用于实现文件中其它源）
#include <queue>                          // 引入 std::queue（连接队列）
#include <mutex>                          // 引入 std::mutex（互斥量）
#include <semaphore.h>                    // 引入 POSIX 信号量 sem_t
#include <thread>                         // 引入 std::thread（可能用于线程相关操作）
#include "../log/log.h"                  // 引入项目内日志模块

class SqlConnPool {                       // 数据库连接池类（单例）
public:                                   // 公有接口
    static SqlConnPool *Instance();       // 获取单例实例的静态方法

    MYSQL *GetConn();                     // 获取一个可用的 MySQL 连接（阻塞/同步）
    void FreeConn(MYSQL * conn);          // 归还一个 MySQL 连接到连接池
    int GetFreeConnCount();               // 查询当前空闲连接数量

    void Init(const char* host, int port, // 初始化连接池：主机、端口、用户、密码、库名、池大小
              const char* user,const char* pwd, 
              const char* dbName, int connSize);
    void ClosePool();                     // 关闭连接池并释放所有连接

private:                                  // 私有成员（外部不可直接构造/析构）
    SqlConnPool();                        // 构造函数（私有用于单例）
    ~SqlConnPool();                       // 析构函数（释放资源）

    int MAX_CONN_;                        // 最大连接数（配置的池大小）
    int useCount_;                        // 已被占用的连接数
    int freeCount_;                       // 当前空闲连接数

    std::queue<MYSQL *> connQue_;         // 连接队列，存放空闲的 MYSQL* 连接
    std::mutex mtx_;                      // 互斥量，保护队列与计数
    sem_t semId_;                         // 信号量，用于控制可用连接的获取
};                                        // SqlConnPool 类定义结束


#endif // SQLCONNPOOL_H                   // 头文件保护宏结束
