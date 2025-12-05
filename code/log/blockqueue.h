#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <sys/time.h>

#include <condition_variable>
#include <deque>
#include <mutex>

template <class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();

    void clear();

    bool empty();  // 队列空了没有

    bool full();  // 队列满了没有

    void Close();

    size_t size();  // 当前容量

    size_t capacity();  // 最大容量
    // 返回任务
    T front();  // 返回队列中等待最久的那个任务（排在队伍第一个）
    T back();   // 返回队列中刚刚入队的那个任务（排在队伍最后一个）

    // 塞任务
    void push_front(const T &item);
    void push_back(const T &item);

    // 做任务
    bool pop(T &item);
    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque<T> deq_;
    size_t capacity_;
    std::mutex mtx_;
    bool isClose_;
    std::condition_variable condConsumer_;
    std::condition_variable condProducer_;
};

template <class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) : capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);
    isClose_ = false;
}

template <class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};

template <class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

template <class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

template <class T>
bool BlockDeque<T>::full() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template <class T>
void BlockDeque<T>::Close() {
    {
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();
        isClose_ = true;
    }
    condProducer_.notify_all();  // 唤醒所有生产者
    condConsumer_.notify_all();  // 唤醒所有消费者
};

template <class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

template <class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

template <class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

template <class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template <class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

template <class T>
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_back(item);
    condConsumer_.notify_one();
}

template <class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.empty()) {
        condConsumer_.wait(locker);
        if (isClose_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}
template <class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.empty()) {
        //chrono::seconds整数 5 转换成一个标准的时间段对象“5秒”。
        //cv_status返回timeout,超时了
        if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) ==
            std::cv_status::timeout) {
            return false;
        }
        if (isClose_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template <class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
};

#endif