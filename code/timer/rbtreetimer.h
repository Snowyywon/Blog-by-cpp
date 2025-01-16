#ifndef RBTREETIMER_H
#define RBTREETIMER_H

#include <set>
#include "../log/log.h"
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include <unordered_map>

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {
    uint64_t id;
    TimeStamp expires;
    TimeoutCallBack cb;

    bool operator<(const TimerNode& rhs) const{
        if(expires < rhs.expires) return true;
        else if(expires > rhs.expires) return false;
        else return id < rhs.id;
    }

};

class RbtreeTimer {

public:
    void adjust(int id,int newExipires);
    void add(int id, int timeOut, const TimeoutCallBack& cb);
    // void doWork(int id);
    void clear();
    void tick();
    void pop();
    int GetNextTick();

private:
    std::set<TimerNode> timerset_;
    std::unordered_map<int, TimerNode> timermap_;
};

#endif