#include <atomic>
#include <vector>
#include <cstdint>
#include <cassert>
#include <thread>

template<class T>
class BlockDeque {
public:
    BlockDeque(size_t capacity = 1000);
    ~BlockDeque();

    void clear();
    bool empty();
    bool full();
    void Close();

    size_t size();
    size_t capacity();

    T front();
    T back();

    void push_back(const T& item);
    void push_front(const T& item);

    bool pop(T& item);
    
    void flush();

private:
    size_t nextIndex(size_t index);
    uint64_t encode(size_t index, uint64_t version);
    std::pair<size_t, uint64_t> decode(uint64_t value);

    size_t capacity_;
    // atomic不支持pair，使用uint64_t把索引和版本合并一起，通过位运算分解
    std::atomic<uint64_t> head_;
    std::atomic<uint64_t> tail_;
    std::vector<T> buffer_;
    std::atomic<bool> is_closed_;
};

template<class T>
BlockDeque<T>::BlockDeque(size_t capacity)
    : capacity_(capacity),
      head_(encode(0, 0)),
      tail_(encode(0, 0)),
      buffer_(capacity),
      is_closed_(false) {
    assert(capacity > 0);
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
}

template<class T>
void BlockDeque<T>::clear() {
    uint64_t version = decode(head_.load()).second + 1;
    head_.store(encode(0, version));
    tail_.store(encode(0, version));
}

template<class T>
bool BlockDeque<T>::empty() {
    return decode(head_.load()).first == decode(tail_.load()).first;
}

template<class T>
bool BlockDeque<T>::full() {
    return nextIndex(decode(tail_.load()).first) == decode(head_.load()).first;
}

template<class T>
void BlockDeque<T>::Close() {
    is_closed_.store(true);
}

template<class T>
size_t BlockDeque<T>::size() {
    size_t head_index = decode(head_.load()).first;
    size_t tail_index = decode(tail_.load()).first;
    return (tail_index + capacity_ - head_index) % capacity_;
}

template<class T>
T BlockDeque<T>::front() {
    assert(!empty() && !is_closed_);
    return buffer_[decode(head_.load()).first];
}

template<class T>
T BlockDeque<T>::back() {
    assert(!empty() && !is_closed_);
    size_t tail_index = decode(tail_.load()).first;
    size_t prev_index = (tail_index == 0) ? capacity_ - 1 : tail_index - 1;
    return buffer_[prev_index];
}

template<class T>
void BlockDeque<T>::push_back(const T& item) {
    while (true) {
        uint64_t current_tail = tail_.load();
        auto [index, version] = decode(current_tail);
        size_t next_index = nextIndex(index);

        if (next_index == decode(head_.load()).first) {
            std::this_thread::yield();
            continue;
        }

        if (tail_.compare_exchange_weak(
                current_tail, encode(next_index, version + 1))) {
            buffer_[index] = item;
            break;
        }
    }
}

template<class T>
void BlockDeque<T>::push_front(const T& item) {
    while (true) {
        uint64_t current_head = head_.load();
        auto [index, version] = decode(current_head);
        size_t next_index = (index == 0) ? capacity_ - 1 : index - 1;

        if (next_index == decode(tail_.load()).first) {
            std::this_thread::yield();
            continue;
        }

        if (head_.compare_exchange_weak(
                current_head, encode(next_index, version + 1))) {
            buffer_[next_index] = item;
            break;
        }
    }
}

template<class T>
bool BlockDeque<T>::pop(T& item) {
    while (true) {
        uint64_t current_head = head_.load();
        auto [index, version] = decode(current_head);

        if (index == decode(tail_.load()).first) {
            std::this_thread::yield();
            continue;
        }

        size_t next_index = nextIndex(index);
        if (head_.compare_exchange_weak(
                current_head, encode(next_index, version + 1))) {
            item = buffer_[index];
            return true;
        }
    }
}

template<class T>
size_t BlockDeque<T>::nextIndex(size_t index) {
    return (index + 1) % capacity_;
}

template<class T>
uint64_t BlockDeque<T>::encode(size_t index, uint64_t version) {
    return (version << 32) | index;
}

template<class T>
std::pair<size_t, uint64_t> BlockDeque<T>::decode(uint64_t value) {
    return {static_cast<size_t>(value & 0xFFFFFFFF), value >> 32};
}

// 由原来的休眠改为了自旋，也就不需要flush了
template<class T>
void BlockDeque<T>::flush() {
    // size_t current_head = head_.load(std::memory_order_acquire);
    // size_t next_index = nextIndex(current_head);

    // if (next_index != decode(tail_.load(std::memory_order_acquire)).first) {
    //     // 唤醒一个消费者，更新头索引指向下一个数据
    //     head_.store(next_index, std::memory_order_release);
    // }
}
