/*
 * @Author       : mark                // 作者
 * @Date         : 2020-06-17          // 创建日期
 * @copyleft Apache 2.0                // 许可协议
 */                                     // 文件头部注释结束
#ifndef HEAP_TIMER_H                    // 头文件保护宏开始
#define HEAP_TIMER_H                    // 定义头文件保护宏

#include <queue>                        // 使用队列（未直接使用于此头，但可能用于实现）
#include <unordered_map>                // 使用哈希表保存 id 到堆索引的映射
#include <time.h>                       // 与时间相关的 C 接口（可能用于实现）
#include <algorithm>                    // 通用算法（如 std::swap 在 .cpp 中使用）
#include <arpa/inet.h>                  // 网络相关头（项目整体可能需要）
#include <functional>                   // std::function 回调类型
#include <assert.h>                     // 断言 assert
#include <chrono>                       // C++ 时间库 chrono
#include "../log/log.h"                // 项目内日志接口

typedef std::function<void()> TimeoutCallBack; // 定义超时回调类型：无参无返回
typedef std::chrono::high_resolution_clock Clock; // 高精度时钟别名
typedef std::chrono::milliseconds MS;     // 毫秒时间单位别名
typedef Clock::time_point TimeStamp;      // 时间点类型别名

struct TimerNode {                        // 定时器节点：存放到期时间与回调
    int id;                               // 定时器唯一标识
    TimeStamp expires;                    // 过期时间点
    TimeoutCallBack cb;                   // 到期触发的回调函数
    bool operator<(const TimerNode& t) {  // 比较运算：按过期时间早晚比较
        return expires < t.expires;       // 到期早的视为更小
    }
};                                        // TimerNode 结束
class HeapTimer {                          // 小根堆定时器（最小到期时间在顶）
public:                                    // 公有接口
    HeapTimer() { heap_.reserve(64); }     // 构造：预留堆容量 64，减少扩容开销

    ~HeapTimer() { clear(); }              // 析构：清空结构
    
    void adjust(int id, int newExpires);   // 调整指定 id 的过期时间（毫秒）

    void add(int id, int timeOut, const TimeoutCallBack& cb); // 添加或更新定时器

    void doWork(int id);                   // 触发并删除指定 id 的定时器

    void clear();                          // 清空所有定时器

    void tick();                           // 执行到期定时器（可能执行多个）

    void pop();                            // 弹出堆顶（最早到期）定时器

    int GetNextTick();                     // 返回距下一次到期的毫秒数（并先执行已到期的）

private:                                   // 私有辅助方法与数据
    void del_(size_t i);                   // 删除堆中索引 i 的节点
    
    void siftup_(size_t i);                // 上滤：使索引 i 的节点上移至正确位置

    bool siftdown_(size_t index, size_t n);// 下滤：从 index 开始向下调整，范围 [0,n)

    void SwapNode_(size_t i, size_t j);    // 交换堆中两个节点并维护映射

    std::vector<TimerNode> heap_;          // 小根堆存储的节点数组

    std::unordered_map<int, size_t> ref_;  // 映射：定时器 id -> 堆索引
};                                         // HeapTimer 结束

#endif //HEAP_TIMER_H                      // 头文件保护宏结束
