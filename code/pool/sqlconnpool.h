#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"  // 日志模块

// 数据库连接池类，用于管理 MySQL 连接的分配和回收
class SqlConnPool {
public:
    // 获取单例实例，确保全局唯一的连接池对象
    static SqlConnPool *Instance();

    // 从连接池中获取一个 MySQL 连接
    MYSQL *GetConn();

    // 将使用完的 MySQL 连接归还到连接池中
    void FreeConn(MYSQL * conn);

    // 获取当前空闲连接的数量
    int GetFreeConnCount();

    // 初始化连接池
    // 参数：数据库主机名、端口号、用户名、密码、数据库名、连接池大小
    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);

    // 关闭连接池，释放所有 MySQL 连接
    void ClosePool();

private:
    // 私有构造函数，单例模式
    SqlConnPool() = default;

    // 析构函数，自动关闭连接池
    ~SqlConnPool() { ClosePool(); }

    int MAX_CONN_;                     // 最大连接数
    std::queue<MYSQL *> connQue_;      // 连接队列，存储空闲的 MySQL 连接
    std::mutex mtx_;                   // 互斥锁，保护连接队列的线程安全
    sem_t semId_;                      // 信号量，用于同步连接的获取与释放
};

/* 资源获取即初始化 (RAII) 类，用于自动管理连接资源的生命周期 */
class SqlConnRAII {
public:
    // 构造函数：从连接池中获取连接，并将其分配给外部指针
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);               // 确保连接池对象非空
        *sql = connpool->GetConn();     // 从连接池中获取连接
        sql_ = *sql;                    // 保存连接指针
        connpool_ = connpool;           // 保存连接池对象
    }
    
    // 析构函数：自动将连接归还到连接池中
    ~SqlConnRAII() {
        if(sql_) { connpool_->FreeConn(sql_); }
    }
    
private:
    MYSQL *sql_;                        // 数据库连接指针
    SqlConnPool* connpool_;             // 连接池对象指针
};

#endif // SQLCONNPOOL_H
