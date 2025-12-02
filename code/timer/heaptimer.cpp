/*
 * @Author       : mark                   // 作者
 * @Date         : 2020-06-17             // 创建日期
 * @copyleft Apache 2.0                   // 许可协议
 */                                        // 文件头部注释结束
#include "heaptimer.h"                     // 引入对应头文件声明
//时间堆：堆是一种完全二叉树结构，小根堆指的是父节点<=子节点
void HeapTimer::siftup_(size_t i) {        // 上滤：将索引 i 的节点上移到合适位置
    assert(i >= 0 && i < heap_.size());    // 断言：i 在堆数组范围内
    size_t j = (i - 1) / 2;                // 计算父结点索引
    while(j >= 0) {                        // 循环比较直到根（注意 size_t 无符号，依赖 break 退出）
        if(heap_[j] < heap_[i]) { break; } // 父节点更小（到期更早）则满足小根堆，停止
        SwapNode_(i, j);                   // 否则与父节点交换
        i = j;                             // 继续向上
        j = (i - 1) / 2;                   // 更新父索引
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) { // 交换堆中两个索引并同步 ref_ 映射
    assert(i >= 0 && i < heap_.size());         // 索引 i 合法
    assert(j >= 0 && j < heap_.size());         // 索引 j 合法
    std::swap(heap_[i], heap_[j]);              // 交换节点
    ref_[heap_[i].id] = i;                      // 更新被换到 i 位置节点的映射
    ref_[heap_[j].id] = j;                      // 更新被换到 j 位置节点的映射
} 

bool HeapTimer::siftdown_(size_t index, size_t n) { // 下滤：从 index 开始在 [0,n) 范围内下移
    assert(index >= 0 && index < heap_.size());     // index 在堆范围内
    assert(n >= 0 && n <= heap_.size());            // 范围 n 合法
    size_t i = index;                               // 当前结点索引
    size_t j = i * 2 + 1;                           // 左孩子索引
    while(j < n) {                                  // 只要存在孩子
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++; // 选择更小的孩子（右孩子更小则用右孩子）
        if(heap_[i] < heap_[j]) break;             // 若当前结点不大于更小的孩子，停止
        SwapNode_(i, j);                            // 否则与更小的孩子交换
        i = j;                                      // 继续向下
        j = i * 2 + 1;                              // 更新左孩子索引
    }
    return i > index;                               // 返回是否发生过移动
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) { // 添加或更新定时器
    assert(id >= 0);                               // 断言 id 合法
    size_t i;                                      // 将被使用的堆索引
    if(ref_.count(id) == 0) {                      // 若不存在此 id 的定时器
        /* 新节点：堆尾插入，调整堆 */              // 说明：插入后执行上滤
        i = heap_.size();                          // 新节点位置在堆尾
        ref_[id] = i;                              // 记录映射
        heap_.push_back({id, Clock::now() + MS(timeout), cb}); // 设置过期时间与回调
        siftup_(i);                                // 上滤使其到正确位置
    } 
    else {                                         // 已存在此 id 的定时器
        /* 已有结点：调整堆 */                      // 说明：更新到期时间与回调后调整
        i = ref_[id];                              // 找到该节点索引
        heap_[i].expires = Clock::now() + MS(timeout); // 更新过期时间
        heap_[i].cb = cb;                          // 更新回调
        if(!siftdown_(i, heap_.size())) {          // 先尝试下滤（若不能下移）
            siftup_(i);                            // 则尝试上滤
        }
    }
}

void HeapTimer::doWork(int id) {                   // 触发并删除指定 id 的定时器
    /* 删除指定id结点，并触发回调函数 */              // 功能说明
    if(heap_.empty() || ref_.count(id) == 0) {     // 若堆空或不存在此 id，直接返回
        return;                                    // 无操作
    }
    size_t i = ref_[id];                           // 获取堆索引
    TimerNode node = heap_[i];                     // 拷贝节点（避免删除前丢失回调）
    node.cb();                                     // 执行回调
    del_(i);                                       // 从堆中删除该节点
}

void HeapTimer::del_(size_t index) {               // 删除堆中指定索引的节点
    /* 删除指定位置的结点 */                         // 功能说明（修正显示乱码的注释）
    assert(!heap_.empty() && index >= 0 && index < heap_.size()); // 索引合法且堆非空
    /* 将要删除的结点换到队尾，然后调整 */            // 先与尾结点交换再处理
    size_t i = index;                               // 当前索引
    size_t n = heap_.size() - 1;                    // 尾结点索引
    assert(i <= n);                                  // 校验 i 不超过尾索引
    if(i < n) {                                     // 若删除的不是尾结点
        SwapNode_(i, n);                            // 交换到队尾
        if(!siftdown_(i, n)) {                      // 尝试下滤剩余堆范围 [0,n)
            siftup_(i);                             // 无法下滤则尝试上滤
        }
    }
    /* 队尾元素删除 */                               // 现在删除队尾（原目标）
    ref_.erase(heap_.back().id);                    // 移除映射
    heap_.pop_back();                               // 弹出队尾元素
}

void HeapTimer::adjust(int id, int timeout) {      // 调整指定 id 的过期时间
    /* 调整指定id的结点 */                           // 功能说明（修正注释）
    assert(!heap_.empty() && ref_.count(id) > 0);   // 必须存在该定时器
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);; // 更新过期时间
    siftdown_(ref_[id], heap_.size());              // 向下调整以维持堆性质
}

void HeapTimer::tick() {                            // 扫描并处理所有已到期定时器
    /* 清除超时结点 */                               // 功能说明
    if(heap_.empty()) {                              // 若无定时器
        return;                                      // 直接返回
    }
    while(!heap_.empty()) {                          // 逐个检查堆顶是否到期
        TimerNode node = heap_.front();              // 取出堆顶（最早到期）
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { // 未到期
            break;                                   // 终止循环
        }
        node.cb();                                   // 到期则执行回调
        pop();                                       // 删除堆顶并继续
    }
}

void HeapTimer::pop() {                              // 删除堆顶元素
    assert(!heap_.empty());                          // 堆必须非空
    del_(0);                                         // 删除索引 0 的节点
}

void HeapTimer::clear() {                            // 清空定时器堆与映射
    ref_.clear();                                    // 清空 id->索引 映射
    heap_.clear();                                   // 清空堆数组
}

int HeapTimer::GetNextTick() {                       // 返回距离下次到期的毫秒值（并执行已到期项）
    tick();                                          // 先处理已到期的定时器
    size_t res = -1;                                 // 默认返回 -1（无定时器时）
    if(!heap_.empty()) {                             // 若仍有定时器
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count(); // 计算剩余时间
        if(res < 0) { res = 0; }                     // 若已到期则返回 0
    }
    return res;                                      // 返回毫秒数
}

