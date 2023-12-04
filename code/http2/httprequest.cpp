#include "httprequest.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include "httplib.h"
// #include <Python.h>
#include <codecvt> 
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", 
             "/text","/tt"};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1}, {"/tt.html", 2} };

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    while(buff.ReadableBytes() && state_ != FINISH) {
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), lineEnd);
        switch(state_)
        {
        case REQUEST_LINE:
            if(!ParseRequestLine_(line)) {
                return false;
            }
            ParsePath_();
            break;    
        case HEADERS:
            ParseHeader_(line);
            if(buff.ReadableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd == buff.BeginWrite()) { break; }
        buff.RetrieveUntil(lineEnd + 2);
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

bool HttpRequest::ParseRequestLine_(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {   
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else {
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void writeToFile(const std::string& filename, const std::string& content) {
    std::ofstream outputFile(filename); // 打开输出文件
    if (!outputFile.is_open()) {
        std::cerr << "无法打开文件" << std::endl;
        return;
    }

    outputFile << content; // 将内容写入文件

    outputFile.close(); // 关闭输出文件
}
std::string urlDecode(const std::string& input) {
    // std::string decodedString;
    // std::istringstream stream(input);
    // char currentChar;

    // while (stream >> std::noskipws >> currentChar) {
    //     if (currentChar == '%') {
    //         char hex[3] = {0};
    //         stream >> hex[0] >> hex[1];
    //         int value;
    //         std::istringstream hexStream(hex);
    //         hexStream >> std::hex >> value;
    //         decodedString += static_cast<char>(value);
    //     } else if (currentChar == '+') {
    //         decodedString += ' ';
    //     } else {
    //         decodedString += currentChar;
    //     }
    // }
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.to_bytes(converter.from_bytes(input));

    // return decodedString;
}
std::string convertToDNA(const std::string& input) {
    std::string dnaSequence;
    for (char c : input) {
        std::bitset<8> bits(c); // 将字符转换为8位二进制
        std::string str = bits.to_string();
        for(int i=0;i<8;i+=2){
            char highBit = str[i+1]; // 高位
            char lowBit = str[i];  // 低位
            if (highBit == '0') {
                if (lowBit == '0') {
                    dnaSequence += "A"; // 00 - A
                } else {
                    dnaSequence += "T"; // 01 - T
                }
            } else {
                if (lowBit == '0') {
                    dnaSequence += "C"; // 10 - C
                } else {
                    dnaSequence += "G"; // 11 - G
                }
            }            
        }

    }
    return dnaSequence;
}
// int callPythonFunction(const char* moduleName, const char* functionName, long arg1, long arg2) {
//     int result = -1; // 默认错误返回值

//     Py_Initialize();

//     PyObject* pModule = PyImport_ImportModule(moduleName);
//     if (pModule) {
//         PyObject* pFunc = PyObject_GetAttrString(pModule, functionName);
//         if (pFunc && PyCallable_Check(pFunc)) {
//             PyObject* pArgs = PyTuple_Pack(2, PyLong_FromLong(arg1), PyLong_FromLong(arg2));
//             PyObject* pValue = PyObject_CallObject(pFunc, pArgs);
//             if (pValue) {
//                 result = PyLong_AsLong(pValue);
//                 Py_DECREF(pValue);
//             } else {
//                 PyErr_Print();
//             }
//             Py_XDECREF(pFunc);
//         } else {
//             PyErr_Print();
//         }
//         Py_DECREF(pModule);
//     } else {
//         PyErr_Print();
//     }

//     Py_Finalize();

//     return result;
// }
void HttpRequest::ParsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag==2){
                path_ = "/tt.html";
                string s=urlDecode(post_["textarea"]);
                s+="\n";
                // s+=post_["textarea"];
                s+=convertToDNA(s);
                writeToFile("/home/siat/yt/WS/WebServer-master/resources/ss.txt", s);
            }
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
    if(body_.size() == 0) { return; }

    string key, value;
    // int num = 0;
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
            // if(key=="textarea"){
            //     i+=2;
            //     break;
            // }
            // num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            // body_[i + 2] = num % 10 + '0';
            // body_[i + 1] = num / 10 + '0';
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
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::Instance());
    assert(sql);
    
    bool flag = false;
    // unsigned int j = 0;
    char order[256] = { 0 };
    // MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, passwd FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);
    // j = mysql_num_fields(res);
    // fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } 
        else { 
            flag = false; 
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, passwd) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}