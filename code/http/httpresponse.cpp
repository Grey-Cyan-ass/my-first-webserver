/*     // 多行注释开始
 * @Author       : mark     // 作者标识
 * @Date         : 2020-06-27     // 文件创建日期
 * @copyleft Apache 2.0     // Apache 2.0 开源许可
 */      // 多行注释结束
#include "httpresponse.h"     // 包含 HTTP 响应生成类的头文件定义

using namespace std;          // 使用标准命名空间，简化 std:: 前缀

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {     // 定义静态常量：文件后缀到 MIME 类型的映射表（无序映射）
    { ".html",  "text/html" },              // HTML 文件对应 text/html 类型
    { ".xml",   "text/xml" },               // XML 文件对应 text/xml 类型
    { ".xhtml", "application/xhtml+xml" },  // XHTML 文件对应 application/xhtml+xml 类型
    { ".txt",   "text/plain" },             // 纯文本文件对应 text/plain 类型
    { ".rtf",   "application/rtf" },        // RTF 文件对应 application/rtf 类型
    { ".pdf",   "application/pdf" },        // PDF 文件对应 application/pdf 类型
    { ".word",  "application/nsword" },     // Word 文件对应 application/nsword 类型
    { ".png",   "image/png" },              // PNG 图片对应 image/png 类型
    { ".gif",   "image/gif" },              // GIF 图片对应 image/gif 类型
    { ".jpg",   "image/jpeg" },             // JPG 图片对应 image/jpeg 类型
    { ".jpeg",  "image/jpeg" },             // JPEG 图片对应 image/jpeg 类型
    { ".au",    "audio/basic" },            // AU 音频对应 audio/basic 类型
    { ".mpeg",  "video/mpeg" },             // MPEG 视频对应 video/mpeg 类型
    { ".mpg",   "video/mpeg" },             // MPG 视频对应 video/mpeg 类型
    { ".avi",   "video/x-msvideo" },        // AVI 视频对应 video/x-msvideo 类型
    { ".gz",    "application/x-gzip" },     // GZIP 压缩文件对应 application/x-gzip 类型
    { ".tar",   "application/x-tar" },      // TAR 归档文件对应 application/x-tar 类型
    { ".css",   "text/css "},               // CSS 样式文件对应 text/css 类型
    { ".js",    "text/javascript "},        // JavaScript 文件对应 text/javascript 类型
};     // 映射表定义结束

const unordered_map<int, string> HttpResponse::CODE_STATUS = {     // 定义静态常量：HTTP 状态码到状态文本的映射表（无序映射）
    { 200, "OK" },              // 200 状态码对应 OK 文本
    { 400, "Bad Request" },     // 400 状态码对应 Bad Request 文本
    { 403, "Forbidden" },       // 403 状态码对应 Forbidden 文本
    { 404, "Not Found" },       // 404 状态码对应 Not Found 文本
};     // 映射表定义结束

const unordered_map<int, string> HttpResponse::CODE_PATH = {     // 定义静态常量：错误状态码到错误页路径的映射表（无序映射）
    { 400, "/400.html" },     // 400 错误对应 /400.html 页面
    { 403, "/403.html" },     // 403 错误对应 /403.html 页面
    { 404, "/404.html" },     // 404 错误对应 /404.html 页面
};     // 映射表定义结束

HttpResponse::HttpResponse() {      // 默认构造函数定义
    code_ = -1;                     // 初始化状态码为 -1（未设置）
    path_ = srcDir_ = "";           // 初始化路径和资源目录为空字符串
    isKeepAlive_ = false;           // 初始化长连接标志为 false
    mmFile_ = nullptr;              // 初始化文件映射指针为空
    mmFileStat_ = { 0 };            // 初始化文件状态结构体为全 0
};     // 构造函数结束

HttpResponse::~HttpResponse() {     // 析构函数定义
    UnmapFile();                    // 调用 UnmapFile 解除内存映射
}     // 析构函数结束

void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code){     // 初始化响应对象的函数定义，参数：资源目录、路径、是否长连接、状态码
    assert(srcDir != "");               // 断言：确保资源目录不为空
    if(mmFile_) { UnmapFile(); }        // 如果已有文件映射，先解除映射
    code_ = code;                       // 设置状态码
    isKeepAlive_ = isKeepAlive;         // 设置长连接标志
    path_ = path;                       // 设置请求路径
    srcDir_ = srcDir;                   // 设置资源根目录
    mmFile_ = nullptr;                  // 重置文件映射指针为空
    mmFileStat_ = { 0 };                // 重置文件状态结构体为全 0
}     // Init 函数结束

void HttpResponse::MakeResponse(Buffer& buff) {                                                 // 生成 HTTP 响应的函数定义，参数：缓冲区引用
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {  // 如果文件不存在或是目录
        code_ = 404;                                                                         // 设置状态码为 404（未找到）
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)) {                                             // 如果文件没有其他用户读权限
        code_ = 403;                                                                         // 设置状态码为 403（禁止访问）
    }
    else if(code_ == -1) {                                                                   // 如果状态码未设置
        code_ = 200;                                                                         // 设置状态码为 200（成功）
    }
    ErrorHtml_();                                                                            // 如果是错误状态码，设置错误页路径
    AddStateLine_(buff);                                                                     // 添加状态行到缓冲区
    AddHeader_(buff);                                                                        // 添加响应头到缓冲区
    AddContent_(buff);                                                                       // 添加响应内容到缓冲区
}     // MakeResponse 函数结束

char* HttpResponse::File() {            // 获取文件映射指针的函数定义
    return mmFile_;                     // 返回文件内存映射指针
}     // File 函数结束

size_t HttpResponse::FileLen() const {      // 获取文件长度的 const 成员函数定义
    return mmFileStat_.st_size;             // 返回文件状态结构体中的文件大小
}     // FileLen 函数结束

void HttpResponse::ErrorHtml_() {                   // 设置错误页路径的私有函数定义
    if(CODE_PATH.count(code_) == 1) {              // 如果状态码在错误页路径映射表中
        path_ = CODE_PATH.find(code_)->second;     // 设置路径为对应的错误页路径
        stat((srcDir_ + path_).data(), &mmFileStat_);     // 更新文件状态信息
    }
}     // ErrorHtml_ 函数结束

void HttpResponse::AddStateLine_(Buffer& buff) {                            // 添加状态行的私有函数定义，参数：缓冲区引用
    string status;                                                          // 定义状态文本变量
    if(CODE_STATUS.count(code_) == 1) {                                     // 如果状态码在状态文本映射表中
        status = CODE_STATUS.find(code_)->second;                           // 获取对应的状态文本
    }
    else {                                                                  // 如果状态码不在映射表中
        code_ = 400;                                                        // 设置状态码为 400
        status = CODE_STATUS.find(400)->second;                             // 获取 400 对应的状态文本
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");   // 将状态行添加到缓冲区（格式：HTTP/1.1 状态码 状态文本\r\n）
}     // AddStateLine_ 函数结束

void HttpResponse::AddHeader_(Buffer& buff) {                       // 添加响应头的私有函数定义，参数：缓冲区引用
    buff.Append("Connection: ");                                // 添加 Connection 字段名到缓冲区
    if(isKeepAlive_) {                                          // 如果是长连接
        buff.Append("keep-alive\r\n");                          // 添加 keep-alive 值
        buff.Append("keep-alive: max=6, timeout=120\r\n");      // 添加 keep-alive 参数（最多 6 个请求，超时 120 秒）
    } else{                                                     // 如果不是长连接
        buff.Append("close\r\n");                               // 添加 close 值
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");    // 添加 Content-Type 字段（值由 GetFileType_ 获取）
}     // AddHeader_ 函数结束

void HttpResponse::AddContent_(Buffer& buff) {                                          // 添加响应内容的私有函数定义，参数：缓冲区引用
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);                           // 以只读方式打开文件
    if(srcFd < 0) {                                                                 // 如果打开失败
        ErrorContent(buff, "File NotFound!");                                       // 生成错误内容页面
        return;                                                                     // 返回
    }

    LOG_DEBUG("file path %s", (srcDir_ + path_).data());                            // 记录文件路径到调试日志
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);     // 将文件映射到内存（只读、私有映射）
    if(*mmRet == -1) {                                                              // 如果映射失败
        ErrorContent(buff, "File NotFound!");                                       // 生成错误内容页面
        return;                                                                     // 返回
    }
    mmFile_ = (char*)mmRet;                                                         // 保存文件映射指针
    close(srcFd);                                                                   // 关闭文件描述符（映射后不再需要）
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n"); // 添加 Content-Length 字段和空行（表示响应头结束）
}     // AddContent_ 函数结束

void HttpResponse::UnmapFile() {                    // 解除文件内存映射的函数定义
    if(mmFile_) {                                   // 如果文件映射指针不为空
        munmap(mmFile_, mmFileStat_.st_size);       // 解除内存映射
        mmFile_ = nullptr;                          // 重置指针为空
    }
}     // UnmapFile 函数结束

string HttpResponse::GetFileType_() {                   // 获取文件 MIME 类型的私有函数定义
    string::size_type idx = path_.find_last_of('.');    // 查找路径中最后一个点号的位置
    if(idx == string::npos) {                           // 如果没有找到点号（无后缀）
        return "text/plain";                            // 返回默认类型 text/plain
    }
    string suffix = path_.substr(idx);                  // 提取文件后缀（包括点号）
    if(SUFFIX_TYPE.count(suffix) == 1) {                // 如果后缀在 MIME 类型映射表中
        return SUFFIX_TYPE.find(suffix)->second;        // 返回对应的 MIME 类型
    }
    return "text/plain";                                // 后缀不在映射表中，返回默认类型
}     // GetFileType_ 函数结束

void HttpResponse::ErrorContent(Buffer& buff, string message)       // 生成错误内容页面的函数定义，参数：缓冲区引用、错误消息
{
    string body;                                                    // 定义 HTML 正文变量
    string status;                                                  // 定义状态文本变量
    body += "<html><title>Error</title>";                           // 添加 HTML 开始标签和标题
    body += "<body bgcolor=\"ffffff\">";                            // 添加 body 标签（白色背景）
    if(CODE_STATUS.count(code_) == 1) {                             // 如果状态码在状态文本映射表中
        status = CODE_STATUS.find(code_)->second;                   // 获取对应的状态文本
    } else {                                                        // 如果状态码不在映射表中
        status = "Bad Request";                                     // 使用默认状态文本
    }
    body += to_string(code_) + " : " + status  + "\n";              // 添加状态码和状态文本
    body += "<p>" + message + "</p>";                               // 添加错误消息段落
    body += "<hr><em>TinyWebServer</em></body></html>";             // 添加分隔线、服务器名称和 HTML 结束标签

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");     // 添加 Content-Length 字段和空行
    buff.Append(body);                                              // 添加 HTML 正文到缓冲区
}     // ErrorContent 函数结束
