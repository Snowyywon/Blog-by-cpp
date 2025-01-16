#include "rbtreetimer.h"

void RbtreeTimer::adjust(int id,int newExipires) {
    auto iter = timermap_.find(id);
    // assert(iter != timermap_.end());

    TimerNode oldNode = iter -> second;
    TimerNode newNode = oldNode;
    newNode.expires = Clock::now() + MS(newExipires);

    timerset_.erase(oldNode);
    timerset_.insert(newNode);
    timermap_[id] = newNode;
}

void RbtreeTimer::add(int id, int timeOut, const TimeoutCallBack& cb) {
    TimerNode node{id, Clock::now() + MS(timeOut),cb};
    timerset_.insert(node);
    timermap_[id] = node;
}

void RbtreeTimer::clear() {
    timerset_.clear();
    timermap_.clear();
}

void RbtreeTimer::tick() {
    if(timerset_.empty())  return ;

    while(!timerset_.empty()) {
        auto iter = timerset_.begin();
        TimerNode node = *iter ;
        TimeoutCallBack cb = node.cb;
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break; 
        }
        node.cb();
        pop();
    }
}

void RbtreeTimer::pop() {
    // assert(!timerset_.empty());
    auto iter = timerset_.begin();
    timermap_.erase(iter -> id);
    timerset_.erase(iter);
}

int RbtreeTimer::GetNextTick() {
    tick();
    size_t res = -1;
    if(!timerset_.empty()) {
        TimerNode node = *timerset_.begin();
        res = std::chrono::duration_cast<MS>(node.expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}