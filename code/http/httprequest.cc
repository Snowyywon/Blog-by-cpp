#include "httprequest.h"

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };


std::string HttpRequest::path() const {
    return path_;
}

std::string& HttpRequest::path() {
    return path_;
}

std::string HttpRequest::method() const{
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    if(!post_.count(key)) {
        return "";
    }
    return post_.find(key) -> second;
}

std::string HttpRequest::GetPost(const char* key) const {
    if(!post_.count(key)) {
        return "";
    }
    return post_.find(key) -> second;
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection") -> second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

std::string  HttpRequest::search(Buffer& buff, const char* &line) {
    const char CRLF[] = "\r\n";
    if(buff.Asc()) {
        line = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        assert(line >= buff.Peek());
        if(buff.Peek() == line) return "";
        std::string s(buff.Peek(), line);
        return s;
    }
    else {
        line = std::search(buff.Peek(), buff.End(), CRLF, CRLF + 2);
        if(line == buff.End()) line = std::search(buff.Begin(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        assert(line >= buff.Begin());
        if(line == buff.BeginWrite()) return "";

        if(line >= buff.Peek()) {
            std::string s(buff.Peek(), line);
            return s;
        }
        else {
            std::string s(buff.Peek(), buff.End()), 
                    s2(buff.Begin(), line);
            s += s2;
            return s;
        }
    }
}

bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    if(buff.ReadableBytes() <= 0) {
        return false;
    }

    while(buff.ReadableBytes() && state_ != FINISH) {
        const char* lastEnd = nullptr;
        std::string s = search(buff, lastEnd);
        // const char* lastEnd = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        // std::string s(buff.Peek(), lastEnd);
        LOG_DEBUG("%s",s.c_str());
        std::cerr << "string is " << s.c_str() << '\n';
        switch(state_) {
            case REQUEST_LINE:
                if(!ParseRequestLine_(s)) {
                    return false;
                }
                ParsePath_();   // 解析路径
                break;    
            case HEADERS:
                ParseHeader_(s);
                if(buff.ReadableBytes() <= 2) { 
                    state_ = FINISH;
                }
                break;
            case BODY:
                ParseBody_(s);
                break;
            default:
                break;
        }
        if(lastEnd == buff.BeginWrite()) { break; } 
        buff.RetrieveUntil(lastEnd + 2);    
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}



void HttpRequest::ParsePath_() {
    if(path_ == "/") {
        path_ = "/index.html";
    }
    else {
       for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const std::string& line) {
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    // 正则匹配成功后的传出参数
    std::smatch subMatch;

    if(std::regex_match(line, subMatch, patten)) {
         method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;   // 状态转换为下一个状态
        return true;
    }
    LOG_ERROR("ParseRequestLine_ Error");
    return false;
}

void HttpRequest::ParseHeader_(const std::string& line) {
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else {
        state_ = BODY;  // 状态转换为下一个状态
    }
}


void HttpRequest::ParseBody_(const std::string& line) {
    body_ = line;
    // get请求一般没有请求体 
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

void HttpRequest::ParsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();  
        // 如果是登录或者注册，需要通过数据库来验证
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second; 
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    path_ = "/error.html";
                }
            }
        }
    }
}

void HttpRequest::ParseFromUrlencoded_() {
    /*
        = 用来分隔key和value
        + 空格
        & 连接符
    */
    if(body_.size() == 0) { return; }

    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

// 16进制转化为10进制
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

bool HttpRequest::UserVerify(const std::string& name, const std::string& pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::Instance());
    assert(sql);
    
    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }
    // 查询密码是否正确
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);
        // 注册且用户名未被使用
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_INFO("pwd error!");
            }
        } 
        else { 
            flag = false; 
            LOG_INFO("user used!");
        }
    }
    mysql_free_result(res);

    // 注册且用户名未被使用
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}