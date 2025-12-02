/* // 多行注释开始
 * @Author       : mark // 作者标识
 * @Date         : 2020-06-25 // 文件创建日期
 * @copyleft Apache 2.0 // Apache 2.0 开源许可
 */  // 多行注释结束
#ifndef HTTP_RESPONSE_H // 防止头文件重复包含的条件编译指令
#define HTTP_RESPONSE_H // 定义头文件保护宏

#include <unordered_map> // 包含无序映射容器（哈希表）
#include <fcntl.h> // 包含文件控制相关函数（如 open）
#include <unistd.h> // 包含 UNIX 标准函数（如 close）
#include <sys/stat.h> // 包含文件状态相关函数（如 stat）
#include <sys/mman.h> // 包含内存映射相关函数（如 mmap、munmap）

#include "../buffer/buffer.h" // 包含缓冲区类头文件
#include "../log/log.h" // 包含日志系统头文件

class HttpResponse { // 定义 HTTP 响应生成类
public: // 公有成员
    HttpResponse(); // 默认构造函数声明
    ~HttpResponse(); // 析构函数声明

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1); // 初始化响应对象的函数声明，参数：资源目录、路径、是否长连接（默认 false）、状态码（默认 -1）
    void MakeResponse(Buffer& buff); // 生成 HTTP 响应的函数声明，参数：缓冲区引用
    void UnmapFile(); // 解除文件内存映射的函数声明
    char* File(); // 获取文件内存映射指针的函数声明
    size_t FileLen() const; // 获取文件长度的 const 成员函数声明
    void ErrorContent(Buffer& buff, std::string message); // 生成错误页内容的函数声明，参数：缓冲区引用、错误消息
    int Code() const { return code_; } // 获取状态码的内联 const 成员函数

private: // 私有成员
    void AddStateLine_(Buffer &buff); // 添加状态行的私有函数声明
    void AddHeader_(Buffer &buff); // 添加响应头的私有函数声明
    void AddContent_(Buffer &buff); // 添加响应内容的私有函数声明

    void ErrorHtml_(); // 设置错误页路径的私有函数声明
    std::string GetFileType_(); // 获取文件 MIME 类型的私有函数声明

    int code_; // HTTP 状态码
    bool isKeepAlive_; // 是否保持长连接标志

    std::string path_; // 请求路径
    std::string srcDir_; // 资源根目录路径
    
    char* mmFile_; // 文件内存映射指针
    struct stat mmFileStat_; // 文件状态信息结构体

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE; // 静态常量：文件后缀到 MIME 类型的映射表
    static const std::unordered_map<int, std::string> CODE_STATUS; // 静态常量：状态码到状态文本的映射表
    static const std::unordered_map<int, std::string> CODE_PATH; // 静态常量：错误码到错误页路径的映射表
}; // 类定义结束


#endif //HTTP_RESPONSE_H // 头文件保护宏结束
