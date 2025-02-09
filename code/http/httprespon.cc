#include "httprespon.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse() {
    code_ = -1;
    isKeepAlive_ = false;
    path_ = srcDir_ = "";
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

HttpResponse::~HttpResponse() {
    UnmapFile();
}

void HttpResponse::Init(const std::string &srcDir, std::string &path, bool isKeepAlive, int code) {
    UnmapFile();

    srcDir_ = srcDir;
    path_ = path;
    isKeepAlive_ = isKeepAlive;
    code_ = code;
}

void HttpResponse::MakeResponse(Buffer &buff) {
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || 
        S_ISDIR(mmFileStat_.st_mode)) { 
        code_ = 404; 
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)){
        code_ = 403;
    }
    else if(code_ == -1) {
        code_ = 200;
    }
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

char *HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorContent(Buffer &buff, std::string message) {
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += std::to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

void HttpResponse::AddStateLine_(Buffer &buff) {
    std::string date = "";
     if(CODE_STATUS.count(code_) == 1) {
        date = CODE_STATUS.find(code_)->second;
    }
    else {
        code_ = 400;
        date = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + date + "\r\n");
}

void HttpResponse::AddHeader_(Buffer &buff) {
    std::string date = "";
    if(isKeepAlive_) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

/*
    我们没必要把文件内容输入到缓冲区
    我们采取mmap文件映射方式，后续再通过writev方法来进行集中写即可
*/
void HttpResponse::AddContent_(Buffer &buff) {
    int fd = open((srcDir_ + path_).data(), O_RDONLY);
    if(fd < 0) {
        ErrorContent(buff, "File NotFound!");
        return; 
    }

    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    void* mmFile = mmap(NULL, FileLen(), PROT_READ, MAP_PRIVATE, fd, 0);
    if(mmFile_ == MAP_FAILED) {
        ErrorContent(buff, "File NOT Found");
        return ;
    }

    mmFile_ = (char *)mmFile;
    close(fd);
    buff.Append("Content-length: " + std::to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

std::string HttpResponse::GetFileType_() {
    std::string::size_type idx = path_.find_last_of('.');
    if(idx == std::string::npos) {
        return "text/plain";
    }
    std::string suffix = path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix) -> second;
    }
    return "text/plain";
}
