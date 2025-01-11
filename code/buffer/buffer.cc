#include "buffer.h"
size_t Buffer::kinitialpreSize = 8;
/*
    readPos_ : 未被读取字符的第一位
    writePos_: 未被写入字符的第一位

    循环vector定义：
    vector为空 : readPos_ == writePos_
    vector为满 : readPos_ == (writePos_ + 1)%size()
*/


Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 可读字节为 [readPos_ , writePos_) 的字节
size_t Buffer::ReadableBytes() const {
    return (writePos_ >= readPos_) ? (writePos_ - readPos_) : (size() - readPos_ + writePos_);
} 
// 可写字节为 [writePos_ , readPos_) 的字节
size_t Buffer::WritableBytes() const {
    return (readPos_ > writePos_) ? (readPos_ - writePos_ - 1) : (size() - writePos_ + readPos_ - 1);
}
// 
size_t Buffer::PrependableBytes() const {
    return 0;
}

const char* Buffer::Peek() const {
    return &buffer_[readPos_];
}

void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ = (readPos_ + len) % size();
}

void Buffer::RetrieveUntil(const char* end) {
    // assert(Peek() <= end);
    ssize_t len = ((end - Peek()) >= 0) ? (end - Peek()) : (((end - Peek()) + size())%size());
    Retrieve(len);
}

void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetrieveAllToStr() {
    ssize_t len = ReadableBytes();
    if(len == 0) {
        return "";
    }
    else if(writePos_ >= readPos_) {
        std::string str(Peek(), ReadableBytes());
        RetrieveAll();
        return str;
    }
    else {
        size_t firLen = size() - readPos_;
        size_t secLen = writePos_;
        std::string str(Peek(), firLen);
        str.append(BeginPtr_(), BeginPtr_() + secLen);
        RetrieveAll();
        return str;
    }
    RetrieveAll();
    return "";
}

const char* Buffer::BeginWriteConst() const {
    return &buffer_[writePos_];
}

char* Buffer::BeginWrite() {
    return &buffer_[writePos_];
}                                                                                                                                                    

void Buffer::HasWritten(size_t len) {
    assert(len <= WritableBytes());
    writePos_ = (writePos_ + len)%size();
} 

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);
    // 如果可以从writePos后存下，就直接copy
    if(readPos_ <= writePos_) {
        size_t firLen = std::min(len, size() - writePos_);
        std::copy(str, str + firLen, BeginWrite());
        if(firLen < len) {
            std::copy(str + firLen + 1, str + len, BeginPtr_());
        }
    }
    else {
        std::copy(str, str + len, BeginWrite());
    }
    assert(len <= WritableBytes());
    HasWritten(len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes()  < len) {
        assert(writePos_ + len + 1 < buffer_.max_size());
        buffer_.resize(writePos_ + len + 1);
    } 
    else {
        size_t readable = ReadableBytes();
        /* 
            如果readPos_ <= writePos_,那就不用修改
            否则我们需要拷贝两次
        */
        if(readPos_ <= writePos_) {
            std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        }
        else {
            std::string s = "";
            if(writePos_ != 0) s.append(BeginPtr_(), BeginPtr_() + writePos_);
            std::copy(BeginPtr_() + readPos_, BeginPtr_() + size(), BeginPtr_());
            std::copy(s.begin(), s.end(), BeginPtr_() + size() - readPos_);
        } 
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}

ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];
    struct iovec iov[3];
    ssize_t cnt = 1;
    const size_t writable = WritableBytes();
    
    /* 
        循环vector的原因，所以我们可能需要采用三个iov
        如果readPos > writePos，就只需要读取[BeginPtr_() + writePos_, BeginPtr_() + readPos_)
        否则，可能需要[BeginPtr_() + writePos_, BeginPtr_() + size()],[BeginPtr_(), BeginPtr_() + readPos_)
    */

    // int flags = fcntl(fd, F_GETFD);
    // assert(flags == -1 && errno == EBADF);
    
    if(readPos_ > writePos_) {
        iov[0].iov_base = BeginPtr_() + writePos_;
        iov[0].iov_len = writable;
        iov[1].iov_base = buff;
        iov[1].iov_len = sizeof(buff);
        cnt = 2;
    }
    else {
        iov[0].iov_base = BeginPtr_() + writePos_;
        iov[0].iov_len = size() - writePos_;
        iov[1].iov_base = BeginPtr_();
        iov[1].iov_len = readPos_;
        
        iov[2].iov_base = buff;
        iov[2].iov_len = sizeof(buff);
        cnt = 3;
    }
    
    const ssize_t len = readv(fd, iov, cnt);
    if (len < 0) {
        *saveErrno = errno;
        // std::cerr << "writeableBytes : " << WritableBytes() << '\n'
        //             << "size : " << size() << "\n" 
        //             << "writePos : " << writePos_ << '\n';

        // std::cerr << "readv failed, errno: " << errno << " (" << strerror(errno) << ")\n";
       
        // for (int i = 0; i < cnt; ++i) {
        //     std::cout << "iov[" << i << "]: base=" << iov[i].iov_base
        //             << ", len=" << iov[i].iov_len << '\n';
        // }
        // assert(len >= 0);
    }
    else if(static_cast<size_t>(len) <= writable) {
        assert(len <= WritableBytes());
        HasWritten(len);
    }
    else {
        HasWritten(writable);
        Append(buff, len - writable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    Retrieve(len);
    // readPos_ += len;
    return len;
}

char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

size_t Buffer::size() const {
    return buffer_.size();
}

const char* Buffer::End() const {
    return BeginPtr_() + size();
}

const bool Buffer::Asc() const {
    return writePos_ >= readPos_;
}
const char* Buffer::Begin() const {
    return BeginPtr_();
}