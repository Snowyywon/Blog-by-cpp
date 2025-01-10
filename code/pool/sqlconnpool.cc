#include "./sqlconnpool.h"

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool sqlconnpol;
    return &sqlconnpol;
}

MYSQL* SqlConnPool::GetConn() {
    if(connQue_.empty()) {
        LOG_WARN("SQL BUSY");
        return nullptr;
    }
    // 信号量v操作
    sem_wait(&semId_);
    std::lock_guard<std::mutex> lock_(mtx_);
    auto conn = connQue_.front();
    connQue_.pop();
    return conn;
}

void SqlConnPool::FreeConn(MYSQL* conn) {
    std::lock_guard<std::mutex> lock_(mtx_);
    connQue_.push(conn);
    sem_post(&semId_);
}

int SqlConnPool::GetFreeConnCount() {
    std::lock_guard<std::mutex> lock_(mtx_);
    return connQue_.size();
}

void SqlConnPool::Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize = 10){
    for(int i = 0; i < connSize; i++) {
        MYSQL* conn = nullptr;
        conn = mysql_init(conn);
        if(!conn) {
            LOG_ERROR("MYSQL INIT ERROR");
        }
        conn = mysql_real_connect(conn, host, user, pwd, dbName, port, nullptr, 0);
        connQue_.emplace(conn);
    }
    MAX_CONN_ = connSize;
    sem_init(&semId_, 0, MAX_CONN_);
}

void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> lock(mtx_);
    while(!connQue_.empty()) {
        auto conn = connQue_.front();
        connQue_.pop();
        mysql_close(conn);
    }
    mysql_library_end();
}

