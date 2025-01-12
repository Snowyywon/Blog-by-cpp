#include "./log.h"

std::mutex Log::mtx_init;
std::shared_ptr<Log> Log::single = nullptr;
std::atomic<bool> Log::log_init{false};

std::shared_ptr<Log> Log::Instance() {
    if(!log_init.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock_(mtx_init);
        if(!log_init.load(std::memory_order_relaxed)) {
            single = std::shared_ptr<Log>(new Log);
            log_init.store(true, std::memory_order_release);
        }
    }
    return single;
}

Log::Log() {
    fp_ = nullptr;
    deque_ = nullptr;
    writeThread_ = nullptr;
    isAsync_ = false;
    toDay_ = 0;
    lineCount_ = 0;
    level_ = 0;
}

Log::~Log() {
    // 把阻塞队列中的任务都取出来做完
    while(!deque_ -> empty()) {
        deque_ -> flush();
    }

    // 关闭阻塞队列
    deque_ -> Close();
    writeThread_ -> join();

    if(fp_) {
        std::lock_guard<std::mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
    // deque_和writeThread_都是智能指针，不需要主动释放
}

void Log::flush() {
    // 判断是否异步
    if(isAsync_) {
        deque_ -> flush();
    }
    fflush(fp_);
}

void Log::FlushLogThread() {
    Instance() -> AsyncWrite_();
}

int Log::GetLevel() {
    return level_;
}

void Log::SetLevel(int level) {
    level_ = level;
}

void Log::write(int level, const char *format,...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    // 日志日期 日志行数  如果不是今天或行数超了
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        std::unique_lock<std::mutex> locker(mtx_);
        locker.unlock();
        
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday)    // 时间不匹配，则替换为最新的日志文件名
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    // 在buffer内生成一条对应的日志信息
    {
        std::unique_lock<std::mutex> locker(mtx_);
        lineCount_++;
        char tmpBuf[128];
        int n = snprintf(tmpBuf, sizeof(tmpBuf), "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        assert(n <= buff_.WritableBytes());
        // buff_.HasWritten(n);
        buff_.Append(tmpBuf, n);
        AppendLogLevelTitle_(level);    

        char tmpBuf1[buff_.WritableBytes()];
        va_start(vaList, format);
        size_t m = vsnprintf(tmpBuf1, buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        // std::cerr << "buff_.WritableBytes() : " << buff_.WritableBytes() << '\n'
        //             << "m : " << m << '\n';
        // if(m > buff_.WritableBytes()) {
        //     std::cerr << tmpBuf1 << '\n';
        // }
        // assert(m <= buff_.WritableBytes());
        
        m = std::min(m, buff_.WritableBytes());
        buff_.Append(tmpBuf1, m);
        // buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        if(isAsync_ && deque_ && !deque_->full()) { // 异步方式（加入阻塞队列中，等待写线程读取日志信息）
            deque_->push_back(buff_.RetrieveAllToStr());
            assert(buff_.ReadableBytes() == 0);
        } else {    // 同步方式（直接向文件中写入日志信息）
            fputs(buff_.Peek(), fp_);   // 同步就直接写入文件
        }
        buff_.RetrieveAll();    // 清空buff
    }
}

// 写线程真正的执行函数
void Log::AsyncWrite_() {
    std::string str = "";
    while(deque_->pop(str)) {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

void Log::init(int level, const char* path, const char* suffix, int maxQueCapacity) {
    isOpen_ = true;
    level_ = level;
    path_ = path;
    suffix_ = suffix;
    if(maxQueCapacity) {    // 异步方式
        isAsync_ = true;
        if(!deque_) {   // 为空则创建一个
            std::unique_ptr<BlockDeque<std::string>> newQue(new BlockDeque<std::string>);
            // 因为unique_ptr不支持普通的拷贝或赋值操作,所以采用move
            // 将动态申请的内存权给deque，newDeque被释放
            deque_ = move(newQue);  // 左值变右值,掏空newDeque

            std::unique_ptr<std::thread> newThread(new std::thread(FlushLogThread));
            writeThread_ = move(newThread);
        }
    } else {
        isAsync_ = false;
    }

    lineCount_ = 0;
    time_t timer = time(nullptr);
    struct tm* systime = localtime(&timer);
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, systime->tm_year + 1900, systime->tm_mon + 1, systime->tm_mday, suffix_);
    toDay_ = systime->tm_mday;
    // std::cerr << fileName << '\n';
    {
        std::lock_guard<std::mutex> locker(mtx_);
        buff_.RetrieveAll();
        if(fp_) {   // 重新打开
            flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName, "a"); // 打开文件读取并附加写入
        if(fp_ == nullptr) {
            mkdir(fileName, 0777);
            fp_ = fopen(fileName, "a"); // 生成目录文件（最大权限）
        }
        // printf("path_: %s\n", path_);
        // printf("fileName: %s\n", fileName);


        if (fp_ == nullptr) {
            perror("fopen failed");
            fprintf(stderr, "Error opening file: %s\n", strerror(errno));
        }

        assert(fp_ != nullptr);
    }
}

// 添加日志等级
void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}
