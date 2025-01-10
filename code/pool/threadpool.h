#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>

class ThreadPool {

public:
    ThreadPool(int threadNumber = 8) : pool_(std::make_shared<Pool>()) {
        for(int i = 0; i < threadNumber; i++) {
            std::thread([this]{
                std::unique_lock<std::mutex> lock_(pool_ -> mtx_);
                while(true) {
                    auto& tasks = pool_ -> tasks;
                    if(!tasks.empty()) {
                        auto task = std::move(tasks.front());
                        tasks.pop();
                        lock_.unlock();
                        task();
                        lock_.lock();
                    }
                    else if(pool_ -> isClose) {
                        break;
                    }
                    else {
                        pool_ -> cv.wait(lock_);
                    }
                }
            }).detach();
        }
    }
    ~ThreadPool() {
        if(pool_) {
            std::unique_lock<std::mutex> locker(pool_ -> mtx_);
            pool_ -> isClose = true;
            pool_ -> cv.notify_all();
        }
    }

    template<typename T>
    void AddTask(T&& task) {
        std::unique_lock<std::mutex> lock(pool_ -> mtx_);
        pool_ -> tasks.emplace(std::forward<T>(task));
        pool_ -> cv.notify_one();
    }

private:
    struct Pool{
        std::mutex mtx_;
        std::condition_variable cv;
        std::queue<std::function<void()>> tasks;
        bool isClose;
    };
    std::shared_ptr<Pool> pool_;
};


#endif