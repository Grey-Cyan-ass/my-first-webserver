/* // 多行注释开始
 * @Author       : mark // 作者标识
 * @Date         : 2020-06-25 // 文件创建日期
 * @copyleft Apache 2.0 // Apache 2.0 开源许可
 */  // 多行注释结束
#ifndef HTTP_REQUEST_H // 防止头文件重复包含的条件编译指令
#define HTTP_REQUEST_H // 定义头文件保护宏

#include <unordered_map> // 包含无序映射容器（哈希表）
#include <unordered_set> // 包含无序集合容器
#include <string> // 包含字符串类
#include <regex> // 包含正则表达式库
#include <errno.h> // 包含错误码定义
#include <mysql/mysql.h> // 包含 MySQL C API 头文件

#include "../buffer/buffer.h" // 包含缓冲区类头文件
#include "../log/log.h" // 包含日志系统头文件
#include "../pool/sqlconnpool.h" // 包含 SQL 连接池头文件
#include "../pool/sqlconnRAII.h" // 包含 SQL 连接 RAII 封装头文件

class HttpRequest { // 定义 HTTP 请求解析类
public: // 公有成员
    enum PARSE_STATE { // 定义解析状态枚举类型
        REQUEST_LINE, // 解析请求行状态
        HEADERS, // 解析请求头状态
        BODY, // 解析请求体状态
        FINISH, // 解析完成状态
    }; // 枚举定义结束

    enum HTTP_CODE { // 定义 HTTP 状态码枚举类型（当前未使用）
        NO_REQUEST = 0, // 无请求
        GET_REQUEST, // GET 请求
        BAD_REQUEST, // 错误请求
        NO_RESOURSE, // 无资源
        FORBIDDENT_REQUEST, // 禁止的请求
        FILE_REQUEST, // 文件请求
        INTERNAL_ERROR, // 内部错误
        CLOSED_CONNECTION, // 连接已关闭
    }; // 枚举定义结束
    
    HttpRequest() { Init(); } // 构造函数：调用 Init 初始化
    ~HttpRequest() = default; // 析构函数：使用编译器默认实现

    void Init(); // 初始化/重置请求对象的函数声明
    bool parse(Buffer& buff); // 解析 HTTP 请求的函数声明，参数：缓冲区引用

    std::string path() const; // 获取请求路径的 const 成员函数声明
    std::string& path(); // 获取请求路径引用的成员函数声明
    std::string method() const; // 获取请求方法的 const 成员函数声明
    std::string version() const; // 获取 HTTP 版本的 const 成员函数声明
    std::string GetPost(const std::string& key) const; // 获取 POST 参数的函数声明（string 参数）
    std::string GetPost(const char* key) const; // 获取 POST 参数的函数声明（C 字符串参数）

    bool IsKeepAlive() const; // 判断是否为长连接的 const 成员函数声明

private: // 私有成员
    bool ParseRequestLine_(const std::string& line); // 解析请求行的私有函数声明
    void ParseHeader_(const std::string& line); // 解析请求头的私有函数声明
    void ParseBody_(const std::string& line); // 解析请求体的私有函数声明

    void ParsePath_(); // 路径规范化的私有函数声明
    void ParsePost_(); // 解析 POST 数据的私有函数声明
    void ParseFromUrlencoded_(); // 解析 URL 编码表单的私有函数声明

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin); // 用户验证的静态函数声明

    PARSE_STATE state_; // 当前解析状态
    std::string method_, path_, version_, body_; // 请求方法、路径、版本、消息体
    std::unordered_map<std::string, std::string> header_; // 请求头映射表（键值对）
    std::unordered_map<std::string, std::string> post_; // POST 参数映射表（键值对）

    static const std::unordered_set<std::string> DEFAULT_HTML; // 静态常量：默认 HTML 路径集合
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG; // 静态常量：HTML 路径到标签的映射
    static int ConverHex(char ch); // 十六进制字符转换函数声明
}; // 类定义结束


#endif //HTTP_REQUEST_H // 头文件保护宏结束
