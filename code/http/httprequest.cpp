/*     // 多行注释开始
 * @Author       : mark     // 作者标识
 * @Date         : 2020-06-26     // 文件创建日期
 * @copyleft Apache 2.0     // Apache 2.0 开源许可
 */      // 多行注释结束
#include "httprequest.h"     // 包含 HTTP 请求解析类的头文件定义
using namespace std;         // 使用标准命名空间，简化 std:: 前缀

const unordered_set<string> HttpRequest::DEFAULT_HTML{     // 定义静态常量：默认 HTML 路径集合（无序集合）
            "/index", "/register", "/login",           // 首页、注册页、登录页路径
             "/welcome", "/video", "/picture", };      // 欢迎页、视频页、图片页路径

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {     // 定义静态常量：HTML 路径到标签 ID 的映射（无序映射）
            {"/register.html", 0}, {"/login.html", 1},  };           // 注册页对应标签 0，登录页对应标签 1

void HttpRequest::Init() {                       // 初始化/重置请求对象的函数定义
    method_ = path_ = version_ = body_ = "";     // 清空方法、路径、版本、消息体字符串
    state_ = REQUEST_LINE;                       // 重置解析状态为解析请求行
    header_.clear();                             // 清空请求头映射表
    post_.clear();                               // 清空 POST 参数映射表
}     // Init 函数结束

bool HttpRequest::IsKeepAlive() const {                                                 // 判断是否为长连接的 const 成员函数定义
    if(header_.count("Connection") == 1) {                                              // 如果请求头中存在 Connection 字段
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1"; // 返回值为 keep-alive 且版本为 1.1 时返回 true
    }
    return false;     // 否则返回 false（不是长连接）
}     // IsKeepAlive 函数结束

bool HttpRequest::parse(Buffer& buff) {                                                         // 解析 HTTP 请求的函数定义，参数：缓冲区引用
    const char CRLF[] = "\r\n";                                                                 // 定义 HTTP 行结束符（回车换行）
    if(buff.ReadableBytes() <= 0) {                                                             // 如果缓冲区中没有可读数据
        return false;                                                                           // 返回 false 表示解析失败
    }
    while(buff.ReadableBytes() && state_ != FINISH) {                                           // 当缓冲区有数据且解析未完成时循环
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);     // 查找行结束符的位置
        std::string line(buff.Peek(), lineEnd);                                                 // 提取一行数据（从当前位置到行结束符）
        switch(state_)                                                                          // 根据当前解析状态进行分支处理
        {
        case REQUEST_LINE:                                                                      // 如果当前状态是解析请求行
            if(!ParseRequestLine_(line)) {                                                      // 调用解析请求行函数
                return false;                                                                   // 如果解析失败，返回 false
            }
            ParsePath_();                                                                       // 解析成功后规范化路径
            break;                                                                              // 跳出 switch
        case HEADERS:                                                                           // 如果当前状态是解析请求头
            ParseHeader_(line);                                                                 // 调用解析请求头函数
            if(buff.ReadableBytes() <= 2) {                                                     // 如果剩余数据不超过 2 字节（只剩空行）
                state_ = FINISH;                                                                // 设置状态为解析完成
            }
            break;                                                                              // 跳出 switch
        case BODY:                                                                              // 如果当前状态是解析请求体
            ParseBody_(line);                                                                   // 调用解析请求体函数
            break;                                                                              // 跳出 switch
        default:                                                                                // 其他状态
            break;                                                                              // 跳出 switch
        }
        if(lineEnd == buff.BeginWrite()) { break; }                                             // 如果已到缓冲区末尾，跳出 while 循环
        buff.RetrieveUntil(lineEnd + 2);                                                        // 从缓冲区中移除已解析的行（包括 CRLF）
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());           // 记录解析结果到调试日志
    return true;                                                                                // 返回 true 表示解析成功
}     // parse 函数结束

void HttpRequest::ParsePath_() {            // 路径规范化的函数定义
    if(path_ == "/") {                      // 如果路径是根路径
        path_ = "/index.html";              // 设置为首页路径
    }
    else {                                  // 如果路径不是根路径
        for(auto &item: DEFAULT_HTML) {     // 遍历默认 HTML 路径集合
            if(item == path_) {             // 如果路径匹配集合中的某项
                path_ += ".html";           // 添加 .html 后缀
                break;                      // 跳出循环
            }
        }
    }
}     // ParsePath_ 函数结束

bool HttpRequest::ParseRequestLine_(const string& line) {       // 解析请求行的函数定义，参数：行字符串
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");             // 定义正则表达式：方法 空格 路径 空格 HTTP/版本
    smatch subMatch;                                            // 定义正则匹配结果对象
    if(regex_match(line, subMatch, patten)) {                   // 如果正则匹配成功
        method_ = subMatch[1];                                  // 提取第一个捕获组（请求方法）
        path_ = subMatch[2];                                    // 提取第二个捕获组（请求路径）
        version_ = subMatch[3];                                 // 提取第三个捕获组（HTTP 版本）
        state_ = HEADERS;                                       // 设置下一个解析状态为解析请求头
        return true;                                            // 返回 true 表示解析成功
    }
    LOG_ERROR("RequestLine Error");                             // 记录错误日志
    return false;                                               // 返回 false 表示解析失败
}     // ParseRequestLine_ 函数结束

void HttpRequest::ParseHeader_(const string& line) {        // 解析请求头的函数定义，参数：行字符串
    regex patten("^([^:]*): ?(.*)$");                       // 定义正则表达式：键 冒号 可选空格 值
    smatch subMatch;                                        // 定·义正则匹配结果对象
    if(regex_match(line, subMatch, patten)) {               // 如果正则匹配成功
        header_[subMatch[1]] = subMatch[2];                 // 将键值对存入请求头映射表
    }
    else {                                                  // 如果匹配失败（遇到空行）
        state_ = BODY;                                      // 设置下一个解析状态为解析请求体
    }
}     // ParseHeader_ 函数结束

void HttpRequest::ParseBody_(const string& line) {              // 解析请求体的函数定义，参数：行字符串
    body_ = line;                                               // 保存消息体内容
    ParsePost_();                                               // 调用 POST 数据解析函数
    state_ = FINISH;                                            // 设置解析状态为完成
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());    // 记录消息体内容和长度到调试日志
}     // ParseBody_ 函数结束

int HttpRequest::ConverHex(char ch) {                       // 十六进制字符转换为数值的静态函数定义，参数：字符
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;     // 如果是大写字母 A-F，返回 10-15
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;     // 如果是小写字母 a-f，返回 10-15
    return ch;                                          // 如果是数字字符，直接返回（ASCII 码值）
}     // ConverHex 函数结束

void HttpRequest::ParsePost_() {                                                                    // 解析 POST 数据的函数定义
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {  // 如果是 POST 请求且内容类型是表单编码
        ParseFromUrlencoded_();                                                                 // 调用 URL 编码解析函数
        if(DEFAULT_HTML_TAG.count(path_)) {                                                     // 如果路径在特殊标签映射表中
            int tag = DEFAULT_HTML_TAG.find(path_)->second;                                     // 获取路径对应的标签 ID
            LOG_DEBUG("Tag:%d", tag);                                                           // 记录标签 ID 到调试日志
            if(tag == 0 || tag == 1) {                                                          // 如果标签是 0（注册）或 1（登录）
                bool isLogin = (tag == 1);                                                      // 标签为 1 时是登录，否则是注册
                if(UserVerify(post_["username"], post_["password"], isLogin)) {                 // 调用用户验证函数
                    path_ = "/welcome.html";                                                    // 验证成功，设置路径为欢迎页
                } 
                else {                                                                          // 验证失败
                    path_ = "/error.html";                                                      // 设置路径为错误页
                }
            }
        }
    }   
}     // ParsePost_ 函数结束

void HttpRequest::ParseFromUrlencoded_() {                              // 解析 URL 编码表单的函数定义
    if(body_.size() == 0) { return; }                                   // 如果消息体为空，直接返回

    string key, value;                                                  // 定义键值变量
    int num = 0;                                                        // 定义十六进制数值变量
    int n = body_.size();                                               // 获取消息体长度
    int i = 0, j = 0;                                                   // 定义循环索引 i 和起始位置 j

    for(; i < n; i++) {                                                 // 遍历消息体的每个字符
        char ch = body_[i];                                             // 获取当前字符
        switch (ch) {                                                   // 根据字符类型进行分支处理
        case '=':                                                       // 如果是等号（键值分隔符）
            key = body_.substr(j, i - j);                               // 提取键名（从 j 到 i）
            j = i + 1;                                                  // 更新起始位置为等号后一位
            break;                                                      // 跳出 switch
        case '+':                                                       // 如果是加号（空格的编码）
            body_[i] = ' ';                                             // 将加号替换为空格
            break;                                                      // 跳出 switch
        case '%':                                                       // 如果是百分号（十六进制编码）
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);     // 计算十六进制值
            body_[i + 2] = num % 10 + '0';                              // 将个位数字转换为字符（此处可能有问题）
            body_[i + 1] = num / 10 + '0';                              // 将十位数字转换为字符
            i += 2;                                                     // 跳过已处理的两个字符
            break;                                                      // 跳出 switch
        case '&':                                                       // 如果是 & 符号（参数分隔符）
            value = body_.substr(j, i - j);                             // 提取值（从 j 到 i）
            j = i + 1;                                                  // 更新起始位置为 & 后一位
            post_[key] = value;                                         // 将键值对存入 POST 参数映射表
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());           // 记录键值对到调试日志
            break;                                                      // 跳出 switch
        default:                                                        // 其他字符
            break;                                                      // 跳出 switch
        }
    }
    assert(j <= i);                                                     // 断言：确保起始位置不超过当前位置
    if(post_.count(key) == 0 && j < i) {                                // 如果键不在映射表中且还有剩余字符（最后一个参数）
        value = body_.substr(j, i - j);                                 // 提取最后一个值
        post_[key] = value;                                             // 将键值对存入映射表
    }
}     // ParseFromUrlencoded_ 函数结束

bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {                         // 用户验证的静态函数定义，参数：用户名、密码、是否登录
    if(name == "" || pwd == "") { return false; }                                                           // 如果用户名或密码为空，返回 false
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());                                           // 记录验证信息到日志
    MYSQL* sql;                                                                                             // 定义 MySQL 连接指针
    SqlConnRAII(&sql,  SqlConnPool::Instance());                                                            // 使用 RAII 方式获取数据库连接
    assert(sql);                                                                                            // 断言：确保连接指针有效
    
    bool flag = false;                                                                                      // 定义验证结果标志，初始为 false
    unsigned int j = 0;                                                                                     // 定义字段数量变量
    char order[256] = { 0 };                                                                                // 定义 SQL 语句缓冲区（256 字节）
    MYSQL_FIELD *fields = nullptr;                                                                          // 定义字段信息指针
    MYSQL_RES *res = nullptr;                                                                               // 定义查询结果集指针
    
    if(!isLogin) { flag = true; }                                                                           // 如果是注册模式，初始假设用户名可用
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());  // 构造 SQL 查询语句
    LOG_DEBUG("%s", order);                                                                                 // 记录 SQL 语句到调试日志

    if(mysql_query(sql, order)) {                                                                           // 执行 SQL 查询
        mysql_free_result(res);                                                                             // 释放结果集（如果有）
        return false;                                                                                       // 查询失败，返回 false
    }
    res = mysql_store_result(sql);                                                                          // 获取查询结果集
    j = mysql_num_fields(res);                                                                              // 获取字段数量
    fields = mysql_fetch_fields(res);                                                                       // 获取字段信息数组

    while(MYSQL_ROW row = mysql_fetch_row(res)) {                                                           // 遍历结果集的每一行
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);                                                      // 记录查询到的用户名和密码
        string password(row[1]);                                                                            // 获取数据库中的密码
        if(isLogin) {                                                                                       // 如果是登录模式
            if(pwd == password) { flag = true; }                                                            // 如果密码匹配，设置标志为 true
            else {                                                                                          // 密码不匹配
                flag = false;                                                                               // 设置标志为 false
                LOG_DEBUG("pwd error!");                                                                    // 记录密码错误日志
            }
        } 
        else {                                                                                              // 如果是注册模式
            flag = false;                                                                                   // 用户名已存在，设置标志为 false
            LOG_DEBUG("user used!");                                                                        // 记录用户名已被使用日志
        }
    }
    mysql_free_result(res);                                                                                 // 释放结果集

    if(!isLogin && flag == true) {                                                                          // 如果是注册模式且用户名可用
        LOG_DEBUG("regirster!");                                                                            // 记录注册日志
        bzero(order, 256);                                                                                  // 清空 SQL 语句缓冲区
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());     // 构造插入 SQL 语句
        LOG_DEBUG( "%s", order);                                                                            // 记录 SQL 语句到调试日志
        if(mysql_query(sql, order)) {                                                                       // 执行插入操作
            LOG_DEBUG( "Insert error!");                                                                    // 插入失败，记录错误日志
            flag = false;                                                                                   // 设置标志为 false
        }
        flag = true;                                                                                        // 注册成功，设置标志为 true
    }
    SqlConnPool::Instance()->FreeConn(sql);                                                                 // 释放数据库连接（RAII 会自动调用）
    LOG_DEBUG( "UserVerify success!!");                                                                     // 记录验证成功日志
    return flag;                                                                                            // 返回验证结果
}     // UserVerify 函数结束

std::string HttpRequest::path() const{      // 获取请求路径的 const 成员函数定义
    return path_;                       // 返回路径字符串
}     // path const 版本函数结束

std::string& HttpRequest::path(){       // 获取请求路径引用的成员函数定义
    return path_;                       // 返回路径引用
}     // path 非 const 版本函数结束

std::string HttpRequest::method() const {       // 获取请求方法的 const 成员函数定义
    return method_;                             // 返回方法字符串
}     // method 函数结束

std::string HttpRequest::version() const {      // 获取 HTTP 版本的 const 成员函数定义
    return version_;                            // 返回版本字符串
}     // version 函数结束

std::string HttpRequest::GetPost(const std::string& key) const {        // 获取 POST 参数的 const 成员函数定义（string 参数版本）
    assert(key != "");                                                  // 断言：确保键不为空
    if(post_.count(key) == 1) {                                         // 如果键在 POST 参数映射表中
        return post_.find(key)->second;                                 // 返回对应的值
    }
    return "";                                                          // 键不存在，返回空字符串
}     // GetPost string 版本函数结束

std::string HttpRequest::GetPost(const char* key) const {       // 获取 POST 参数的 const 成员函数定义（C 字符串参数版本）
    assert(key != nullptr);                                     // 断言：确保指针不为空
    if(post_.count(key) == 1) {                                 // 如果键在 POST 参数映射表中
        return post_.find(key)->second;                         // 返回对应的值
    }
    return "";                                                  // 键不存在，返回空字符串
}     // GetPost C 字符串版本函数结束
