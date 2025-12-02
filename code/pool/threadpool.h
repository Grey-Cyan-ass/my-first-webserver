/*
 * @Author       : mark                     // 作者信息
 * @Date         : 2020-06-15               // 创建日期
 * @copyleft Apache 2.0                     // 许可协议
 */                                          // 文件头部注释结束

#ifndef THREADPOOL_H                          // 头文件保护宏开始，防止重复包含
#define THREADPOOL_H                          // 定义保护宏

#include <mutex>                              // 使用 std::mutex 互斥量
#include <condition_variable>                 // 使用 std::condition_variable 条件变量
#include <queue>                              // 使用 std::queue 任务队列
#include <thread>                             // 使用 std::thread 线程
#include <functional>                         // 使用 std::function 存放任务
// 注意：本文件中使用了 assert 与 std::shared_ptr/std::make_shared，
// 需要在某处包含 <cassert> 与 <memory>（可能由其他头间接包含）。

class ThreadPool {                            // 线程池类定义开始
public:                                       // 公有成员区域
    explicit ThreadPool(size_t threadCount = 8) // 构造函数：默认创建 8 个工作线程
        : pool_(std::make_shared<Pool>()) {   // 初始化共享的内部状态池对象
            assert(threadCount > 0);           // 断言线程数必须大于 0
            for(size_t i = 0; i < threadCount; i++) { // 循环创建指定数量的线程
                std::thread([pool = pool_] {   // 启动线程，按值捕获共享池指针，避免悬垂
                    std::unique_lock<std::mutex> locker(pool->mtx); // 加互斥锁，保护共享数据
                    while(true) {              // 工作循环：不断处理任务或等待
                        if(!pool->tasks.empty()) {           // 若队列非空，有任务可执行
                            auto task = std::move(pool->tasks.front()); // 取出队首任务并移动
                            pool->tasks.pop();               // 弹出队首任务
                            locker.unlock();                 // 解锁再执行，避免长时间占锁
                            task();                          // 执行任务
                            locker.lock();                   // 执行完毕后重新加锁
                        } 
                        else if(pool->isClosed) break;       // 若线程池已关闭，退出循环
                        else pool->cond.wait(locker);        // 否则无任务，等待条件变量唤醒
                    }                                        // while(true) 结束（线程退出）
                }).detach();                                 // 分离线程：无需手动 join
            }                                                // for 循环结束
    }                                                        // 构造函数体结束

    ThreadPool() = default;                  // 默认构造（与上方构造共存可能引发二义性）

    ThreadPool(ThreadPool&&) = default;      // 移动构造函数：允许移动线程池对象
    
    ~ThreadPool() {                          // 析构函数：通知所有工作线程退出
        if(static_cast<bool>(pool_)) {       // 若共享池有效
            {                                // 作用域：用于控制锁的生命周期
                std::lock_guard<std::mutex> locker(pool_->mtx); // 加锁保护共享状态
                pool_->isClosed = true;      // 设置关闭标志，令工作线程停止
            }                                // 解锁（locker 离开作用域自动释放）
            pool_->cond.notify_all();        // 唤醒所有等待中的线程以便退出
        }                                    // if 结束
    }                                        // 析构函数结束

    template<class F>                        // 模板：接受任意可调用对象类型 F
    void AddTask(F&& task) {                 // 向线程池提交一个任务
        {
            std::lock_guard<std::mutex> locker(pool_->mtx); // 加锁以安全访问任务队列
            pool_->tasks.emplace(std::forward<F>(task));    // 完美转发，存入队列
        }                                                   // 解锁（作用域结束）
        pool_->cond.notify_one();           // 通知一个等待线程有新任务可执行
    }                                       // AddTask 结束

private:                                    // 私有成员区域
    struct Pool {                           // 共享状态结构体（在线程间共享）
        std::mutex mtx;                     // 互斥量：保护任务队列和状态
        std::condition_variable cond;       // 条件变量：用于等待/通知
        bool isClosed;                      // 关闭标志：true 表示线程池要退出（未初始化）
        std::queue<std::function<void()>> tasks; // 任务队列：存放无参、无返回任务
    };                                      // Pool 结构体结束
    std::shared_ptr<Pool> pool_;            // 指向共享状态的智能指针
};                                          // ThreadPool 类定义结束


#endif //THREADPOOL_H                        // 头文件保护宏结束
