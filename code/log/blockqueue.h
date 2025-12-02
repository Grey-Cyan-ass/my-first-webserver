/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft Apache 2.0
 */ 

// 防止头文件重复包含
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

// 引入必要的标准库头文件
#include <mutex>              // 互斥量，用于线程同步
#include <deque>             // 双端队列容器
#include <condition_variable> // 条件变量，用于线程通信
#include <sys/time.h>        // 系统时间相关功能

// 定义一个模板类BlockDeque，实现一个线程安全的阻塞队列
template<class T>
class BlockDeque {
public:
    // 构造函数，指定队列最大容量，默认为1000
    explicit BlockDeque(size_t MaxCapacity = 1000);

    // 析构函数
    ~BlockDeque();

    // 清空队列中的所有元素
    void clear();

    // 判断队列是否为空
    bool empty();

    // 判断队列是否已满
    bool full();

    // 关闭队列，停止所有操作
    void Close();

    // 获取队列当前元素个数
    size_t size();

    // 获取队列的最大容量
    size_t capacity();

    // 获取队首元素
    T front();

    // 获取队尾元素
    T back();

    // 在队尾添加元素
    void push_back(const T &item);

    // 在队首添加元素
    void push_front(const T &item);

    // 从队列中弹出元素到item中
    bool pop(T &item);

    // 带超时的弹出操作，超时时间为timeout秒
    bool pop(T &item, int timeout);

    // 唤醒一个等待的消费者线程
    void flush();

private:
    std::deque<T> deq_;                    // 实际存储数据的双端队列
    size_t capacity_;                      // 队列最大容量
    std::mutex mtx_;                       // 互斥锁，保护共享数据
    bool isClose_;                         // 队列是否关闭的标志
    std::condition_variable condConsumer_; // 消费者条件变量
    std::condition_variable condProducer_; // 生产者条件变量
};


// 构造函数实现
template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    // 确保容量大于0
    assert(MaxCapacity > 0);
    // 初始化关闭标志为false
    isClose_ = false;
}

// 析构函数实现，调用Close关闭队列
template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};

// 关闭队列的实现
template<class T>
void BlockDeque<T>::Close() {
    {   
        // 加锁保护共享数据
        std::lock_guard<std::mutex> locker(mtx_);
        // 清空队列
        deq_.clear();
        // 设置关闭标志
        isClose_ = true;
    }
    // 唤醒所有等待的生产者和消费者
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

// 唤醒一个等待的消费者
template<class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
};

// 清空队列实现
template<class T>
void BlockDeque<T>::clear() {
    // 加锁保护共享数据
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

// 获取队首元素实现
template<class T>
T BlockDeque<T>::front() {
    // 加锁保护共享数据
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

// 获取队尾元素实现
template<class T>
T BlockDeque<T>::back() {
    // 加锁保护共享数据
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

// 获取当前队列大小
template<class T>
size_t BlockDeque<T>::size() {
    // 加锁保护共享数据
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

// 获取队列容量
template<class T>
size_t BlockDeque<T>::capacity() {
    // 加锁保护共享数据
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

// 从队尾添加元素实现
template<class T>
void BlockDeque<T>::push_back(const T &item) {
    // 使用unique_lock以便能配合条件变量
    std::unique_lock<std::mutex> locker(mtx_);
    // 当队列满时，等待队列有空间
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    // 添加元素到队尾
    deq_.push_back(item);
    // 唤醒一个等待的消费者
    condConsumer_.notify_one();
}

// 从队首添加元素实现
template<class T>
void BlockDeque<T>::push_front(const T &item) {
    // 使用unique_lock以便能配合条件变量
    std::unique_lock<std::mutex> locker(mtx_);
    // 当队列满时，等待队列有空间
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    // 添加元素到队首
    deq_.push_front(item);
    // 唤醒一个等待的消费者
    condConsumer_.notify_one();
}

// 判断队列是否为空
template<class T>
bool BlockDeque<T>::empty() {
    // 加锁保护共享数据
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

// 判断队列是否已满
template<class T>
bool BlockDeque<T>::full(){
    // 加锁保护共享数据
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

// 从队列中弹出元素的实现
template<class T>
bool BlockDeque<T>::pop(T &item) {
    // 使用unique_lock以便能配合条件变量
    std::unique_lock<std::mutex> locker(mtx_);
    // 当队列为空时，等待有数据
    while(deq_.empty()){
        condConsumer_.wait(locker);
        // 如果队列已关闭，返回失败
        if(isClose_){
            return false;
        }
    }
    // 取出队首元素
    item = deq_.front();
    deq_.pop_front();
    // 唤醒一个等待的生产者
    condProducer_.notify_one();
    return true;
}

// 带超时的弹出操作实现
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    // 使用unique_lock以便能配合条件变量
    std::unique_lock<std::mutex> locker(mtx_);
    // 当队列为空时，等待有数据
    while(deq_.empty()){
        // 等待指定时间，如果超时返回false
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){
            return false;
        }
        // 如果队列已关闭，返回失败
        if(isClose_){
            return false;
        }
    }
    // 取出队首元素
    item = deq_.front();
    deq_.pop_front();
    // 唤醒一个等待的生产者
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H