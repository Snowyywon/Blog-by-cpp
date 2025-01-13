#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <atomic>
#include <vector>
#include <thread>
#include <cassert>
#include <chrono>

/**
 * @brief 无锁阻塞队列的实现。
 * @tparam T 队列中存储的元素类型。
 */
template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t capacity = 1000);
    ~BlockDeque();

    void clear();          // 清空队列
    bool empty();          // 检查队列是否为空
    bool full();           // 检查队列是否已满
    void Close();          // 关闭队列

    size_t size();         // 获取当前队列的大小
    size_t capacity();     // 获取队列的最大容量

    T front();             // 获取队列的第一个元素
    T back();              // 获取队列的最后一个元素

    void push_back(const T& item);         // 向队列尾部添加元素
    void push_front(const T& item);        // 向队列头部添加元素

    bool pop(T& item);                     // 从队列头部取出元素
    bool pop(T& item, int timeout);        // 带超时的从队列头部取出元素
    void flush();                         // 唤醒消费者线程

private:
    size_t nextIndex(size_t index);        // 计算环形缓冲区的下一个索引

    size_t capacity_;                      ///< 队列的最大容量
    std::atomic<size_t> head_;             ///< 队列的头部索引
    std::atomic<size_t> tail_;             ///< 队列的尾部索引
    std::atomic<bool> is_closed_;          ///< 队列关闭标志
    std::vector<T> buffer_;                ///< 用于存储元素的环形缓冲区
};

template<class T>
BlockDeque<T>::BlockDeque(size_t capacity)
    : capacity_(capacity), head_(0), tail_(0), is_closed_(false), buffer_(capacity) {
    assert(capacity > 0);
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
}

template<class T>
void BlockDeque<T>::clear() {
    head_.store(0, std::memory_order_release);
    tail_.store(0, std::memory_order_release);
}

template<class T>
bool BlockDeque<T>::empty() {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
}

template<class T>
bool BlockDeque<T>::full() {
    return nextIndex(tail_.load(std::memory_order_acquire)) == head_.load(std::memory_order_acquire);
}

template<class T>
void BlockDeque<T>::Close() {
    is_closed_.store(true, std::memory_order_release);
}

template<class T>
size_t BlockDeque<T>::size() {
    size_t current_head = head_.load(std::memory_order_acquire);
    size_t current_tail = tail_.load(std::memory_order_acquire);
    return (current_tail + capacity_ - current_head) % capacity_;
}

template<class T>
size_t BlockDeque<T>::capacity() {
    return capacity_;
}

template<class T>
T BlockDeque<T>::front() {
    assert(!empty() && !is_closed_);
    return buffer_[head_.load(std::memory_order_acquire)];
}

template<class T>
T BlockDeque<T>::back() {
    assert(!empty() && !is_closed_);
    size_t tail = tail_.load(std::memory_order_acquire);
    size_t prev_tail = (tail == 0) ? capacity_ - 1 : tail - 1;
    return buffer_[prev_tail];
}

template<class T>
void BlockDeque<T>::push_back(const T& item) {
    while (true) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = nextIndex(current_tail);

        if (next_tail == head_.load(std::memory_order_acquire)) {
            if (is_closed_.load(std::memory_order_acquire)) {
                throw std::runtime_error("Queue is closed");
            }
            std::this_thread::yield(); // 队列满时自旋等待
            continue;
        }

        if (tail_.compare_exchange_weak(current_tail, next_tail, std::memory_order_release)) {
            buffer_[current_tail] = item;
            break;
        }
    }
}

template<class T>
void BlockDeque<T>::push_front(const T& item) {
    while (true) {
        size_t current_head = head_.load(std::memory_order_relaxed);
        size_t next_head = (current_head == 0) ? capacity_ - 1 : current_head - 1;

        if (next_head == tail_.load(std::memory_order_acquire)) {
            if (is_closed_.load(std::memory_order_acquire)) {
                throw std::runtime_error("Queue is closed");
            }
            std::this_thread::yield(); // 队列满时自旋等待
            continue;
        }

        if (head_.compare_exchange_weak(current_head, next_head, std::memory_order_release)) {
            buffer_[current_head] = item;
            break;
        }
    }
}

template<class T>
bool BlockDeque<T>::pop(T& item) {
    while (true) {
        size_t current_head = head_.load(std::memory_order_relaxed);

        if (current_head == tail_.load(std::memory_order_acquire)) {
            if (is_closed_.load(std::memory_order_acquire)) {
                return false;
            }
            std::this_thread::yield(); // 队列空时自旋等待
            continue;
        }

        if (head_.compare_exchange_weak(current_head, nextIndex(current_head), std::memory_order_release)) {
            item = buffer_[current_head];
            return true;
        }
    }
}

template<class T>
bool BlockDeque<T>::pop(T& item, int timeout) {
    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        size_t current_head = head_.load(std::memory_order_relaxed);

        if (current_head == tail_.load(std::memory_order_acquire)) {
            if (is_closed_.load(std::memory_order_acquire)) {
                return false;
            }

            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= timeout) {
                return false;
            }
            std::this_thread::yield(); // 队列空时自旋等待
            continue;
        }

        if (head_.compare_exchange_weak(current_head, nextIndex(current_head), std::memory_order_release)) {
            item = buffer_[current_head];
            return true;
        }
    }
}

template<class T>
void BlockDeque<T>::flush() {
    size_t tail = tail_.load(std::memory_order_acquire);
    size_t prev_tail = (tail == 0) ? capacity_ - 1 : tail - 1;
    head_.store(prev_tail, std::memory_order_release);
}

template<class T>
size_t BlockDeque<T>::nextIndex(size_t index) {
    return (index + 1) % capacity_;
}

#endif // LOCKFREE_BLOCKQUEUE_H
