#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

/**
 * @brief 一个线程安全的阻塞双端队列实现。
 * @tparam T 队列中存储的元素类型。
 */
template<class T>
class BlockDeque {
public:
    /**
     * @brief 构造函数，用于初始化阻塞队列的最大容量。
     * @param MaxCapacity 队列的最大容量，默认为 1000。
     */
    explicit BlockDeque(size_t MaxCapacity = 1000);

    /**
     * @brief 析构函数，用于释放资源。
     */
    ~BlockDeque();

    /**
     * @brief 清空队列中的所有元素。
     */
    void clear();

    /**
     * @brief 检查队列是否为空。
     * @return 如果队列为空，返回 true；否则返回 false。
     */
    bool empty();

    /**
     * @brief 检查队列是否已满。
     * @return 如果队列已满，返回 true；否则返回 false。
     */
    bool full();

    /**
     * @brief 关闭队列，并唤醒所有等待线程。
     */
    void Close();

    /**
     * @brief 获取队列中当前的元素数量。
     * @return 队列中元素的数量。
     */
    size_t size();

    /**
     * @brief 获取队列的最大容量。
     * @return 队列的最大容量。
     */
    size_t capacity();

    /**
     * @brief 获取队列中的第一个元素。
     * @return 队列的第一个元素。
     */
    T front();

    /**
     * @brief 获取队列中的最后一个元素。
     * @return 队列的最后一个元素。
     */
    T back();

    /**
     * @brief 向队列尾部添加一个元素。如果队列已满，会阻塞等待。
     * @param item 要添加的元素。
     */
    void push_back(const T &item);

    /**
     * @brief 向队列头部添加一个元素。如果队列已满，会阻塞等待。
     * @param item 要添加的元素。
     */
    void push_front(const T &item);

    /**
     * @brief 从队列头部移除并获取一个元素。如果队列为空，会阻塞等待。
     * @param item 引用参数，用于存储获取的元素。
     * @return 如果成功获取元素，返回 true；如果队列已关闭，返回 false。
     */
    bool pop(T &item);

    /**
     * @brief 从队列头部移除并获取一个元素（带超时时间）。如果队列为空，会阻塞等待直到超时。
     * @param item 引用参数，用于存储获取的元素。
     * @param timeout 超时时间，单位为秒。
     * @return 如果成功获取元素，返回 true；如果超时或队列已关闭，返回 false。
     */
    bool pop(T &item, int timeout);

    /**
     * @brief 唤醒一个等待获取数据的消费者线程。
     */
    void flush();

private:
    std::deque<T> deq_;                      ///< 内部存储元素的双端队列。
    size_t capacity_;                        ///< 队列的最大容量。
    std::mutex mtx_;                         ///< 用于保证线程安全的互斥锁。
    bool isClose_;                           ///< 标记队列是否已关闭。
    std::condition_variable condConsumer_;  ///< 消费者线程等待条件变量。
    std::condition_variable condProducer_;  ///< 生产者线程等待条件变量。
};

template<typename T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) : 
                            capacity_(MaxCapacity), isClose_(false) {
    assert(MaxCapacity > 0);
}

template<typename T>
BlockDeque<T>::~BlockDeque() {
    Close();
}

template<typename T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> lock_(mtx_);
    deq_.clear();
}

template<typename T>
void BlockDeque<T>::Close() {
    {
        std::lock_guard<std::mutex> lock_(mtx_);
        deq_.clear();
        isClose_ = true;
    }

    // 还需要把睡眠的生产者都唤醒
    condProducer_.notify_all();
    condConsumer_.notify_all();
}

template<typename T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> lock_(mtx_);
    return deq_.empty();
}

template<typename T>
bool BlockDeque<T>::full() {
    std::lock_guard<std::mutex> lock_(mtx_);
    return deq_.size() == capacity_;
}

template<typename T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> lock_(mtx_);
    return deq_.size();
}

template<typename T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> lock_(mtx_);
    return capacity_;
}

template<typename T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locke_(mtx_);
    return deq_.front();
}

template<typename T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locke_(mtx_);
    return deq_.back();
}

template<typename T>
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> lock_(mtx_);
    // 队列满了，就阻塞生产
    while(capacity_ <= deq_.size()) {
        condProducer_.wait(lock_);
    }
    deq_.push_back(item);
    condConsumer_.notify_one();
}

template<typename T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> lock_(mtx_);
    // 队列满了，就阻塞生产
    while(capacity_ <= deq_.size()) {
        condProducer_.wait(lock_);
    }
    deq_.push_back(item);
    condConsumer_.notify_one();
}

template<typename T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> lock_(mtx_);
    // 队列空了，就停止生成
    while(deq_.size() == 0) {
        condConsumer_.wait(lock_);
        if(isClose_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<typename T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> lock_(mtx_);
    // 队列空了，就停止生成
    while(deq_.size() == 0) {
        if(isClose_ || condConsumer_.wait_for(lock_, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<typename T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
}


#endif // BLOCKQUEUE_H
