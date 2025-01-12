#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         //mkdir
#include <atomic>
#include <memory>  
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log {
public:
    // 初始化日志实例（阻塞队列最大容量、日志保存路径、日志文件后缀）
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static std::shared_ptr<Log> Instance();
    // 异步写日志公有方法，调用私有方法asyncWrite
    static void FlushLogThread();   

    // 将输出内容按照标准格式整理
    void write(int level, const char *format,...);
    // 唤醒阻塞队列消费者，开始写日志  
    void flush();   

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }
    virtual ~Log();

private:
    Log();
    void AppendLogLevelTitle_(int level);
    
    // 异步写日志方法
    void AsyncWrite_(); 

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;
    static std::mutex mtx_init;
    static std::atomic<bool> log_init;
    static std::shared_ptr<Log> single;

    const char* path_;
    const char* suffix_;

    int MAX_LINES_;

    int lineCount_;
    int toDay_;

    bool isOpen_;
 
    Buffer buff_;
    int level_;
    bool isAsync_;

    FILE* fp_;
    std::unique_ptr<BlockDeque<std::string>> deque_; 
    std::unique_ptr<std::thread> writeThread_;
    std::mutex mtx_;
    
};

#define LOG_BASE(level, format, ...) \
    do {\
        std::shared_ptr<Log> log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H