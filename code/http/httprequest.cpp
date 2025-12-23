#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};

void HttpRequest::Init() {
  method_ = path_ = version_ = body_ = "";
  state_ = REQUEST_LINE;  // state_固定设定为请求头(第一个state_)
  header_.clear();
  post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
  if (header_.count("Connection") == 1) {
    return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
  }
  return false;
}

bool HttpRequest::parse(Buffer& buff) {  // 解析请求行
  const char CRLF[] = "\r\n";            // 定义一个换行符
  if (buff.ReadableBytes() <= 0) {
    return false;
  }
  while (buff.ReadableBytes() && state_ != FINISH) {
    const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
    std::string line(buff.Peek(), lineEnd);  // 提取这一行
    switch (state_) {                        // 解析对应的行
      case REQUEST_LINE:
        if (!ParseRequestLine_(line)) {
          return false;
        }
        ParsePath_();
        break;
      case HEADERS:
        ParseHeader_(line);
        if (buff.ReadableBytes() <= 2) {
          state_ = FINISH;
        }
        break;
      case BODY:
        ParseBody_(line);
        break;
      default:
        break;
    }
    if (lineEnd == buff.BeginWrite()) {
      break;
    }
    buff.RetrieveUntil(lineEnd + 2);  // peek指针后移
  }
  LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
  return true;
}

void HttpRequest::ParsePath_() {
  if (path_ == "/") {
    path_ = "/index.html";
  } else {
    for (auto& item : DEFAULT_HTML) {
      if (item == path_) {
        path_ += ".html";
        break;
      }
    }
  }
}

bool HttpRequest::ParseRequestLine_(const string& line) {
  regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
  smatch subMatch;
  if (regex_match(line, subMatch, patten)) {
    // 解析出三个变量
    method_ = subMatch[1];
    path_ = subMatch[2];
    version_ = subMatch[3];
    state_ = HEADERS;  // 把state往下移一位
    return true;
  }
  LOG_ERROR("RequestLine Error");
  return false;
}

void HttpRequest::ParseHeader_(const string& line) {
  regex patten("^([^:]*): ?(.*)$");
  smatch subMatch;
  // 要匹配的文本,存储的对象,匹配的格式
  if (regex_match(line, subMatch, patten)) {
    header_[subMatch[1]] = subMatch[2];
  } else {
    state_ = BODY;
  }
}

void HttpRequest::ParseBody_(const string& line) {
  body_ = line;
  ParsePost_();  // 解析body(本项目body只会在登录或者注册状态下携带用户名和密码)
  state_ = FINISH;
  LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

int HttpRequest::ConverHex(char ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  return -1; // 非法字符
}


void HttpRequest::ParsePost_() {
  if (method_ == "POST" &&  // 如果是登录或者注册
      header_["Content-Type"] == "application/x-www-form-urlencoded") {
    ParseFromUrlencoded_();               // 把body解析出来
    if (DEFAULT_HTML_TAG.count(path_)) {  // 找到当前页面对应的tag
      int tag = DEFAULT_HTML_TAG.find(path_)->second;
      LOG_DEBUG("Tag:%d", tag);
      if (tag == 0 || tag == 1) {
        bool isLogin = (tag == 1);         // isLogin赋值为是否登录
        if (UserVerify(post_["username"],  // 进行注册或登录
                       post_["password"], isLogin)) {
          path_ = "/welcome.html";
        } else {
          path_ = "/error.html";
        }
      }
    }
  }
}

// 把body(即username和password解析出来)
void HttpRequest::ParseFromUrlencoded_() {
  if (body_.size() == 0) {
    return;
  }

  string key, value, temp;
  int n = body_.size();

  for (int i = 0; i < n; i++) {
    char ch = body_[i];
    if (ch == '=') {
      key = temp;
      temp.clear();
    } else if (ch == '&') {
      post_[key] = temp;
      temp.clear();
    } else if (ch == '+') {
      temp += ' ';
    } else if (ch == '%' && i + 2 < n) {
      int num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
      temp += static_cast<char>(num);
      i += 2;
    }else{
        temp += ch;
    }
  }
  if(!key.empty()){
    post_[key] = temp;//把 当前还没来得及存的 value（temp）补存进去
  }
}

bool HttpRequest::UserVerify(const string& name, const string& pwd, bool isLogin) {
  if (name == "" || pwd == "") {
    return false;
  }
  LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
  MYSQL* sql;
  SqlConnRAII ConnRAII(&sql, SqlConnPool::Instance());
  assert(sql);

  bool flag = false;
  unsigned int j = 0;
  char order[256] = {0};
  MYSQL_FIELD* fields = nullptr;
  MYSQL_RES* res = nullptr;

  if (!isLogin) {  // 注册状态
    flag = true;   // 允许注册
  }
  // 将格式化的 SQL 查询语句写入 order 数组
  snprintf(order, 256,
           "SELECT username, password FROM user WHERE "
           "username='%s' LIMIT 1",
           name.c_str());
  LOG_DEBUG("%s", order);

  // mysql_query非0,执行失败(表示程序出错了,不是代表没查到)
  if (mysql_query(sql, order)) {
    return false;
  }

  // 结果存放进res,如果没有查到用户名,res里存放的就是空集
  res = mysql_store_result(sql);
  j = mysql_num_fields(res);         // 获取字段的数量(username和password,一般为2)
  fields = mysql_fetch_fields(res);  // 获取所有字段的详细信息

  while (MYSQL_ROW row = mysql_fetch_row(res)) {  // 取一行数据
    LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
    string password(row[1]);  // row1赋值给password
    if (isLogin) {            // 如果是登录状态
      if (pwd == password) {  // 密码正确,允许登录
        flag = true;
      } else {  // 密码错误,报错
        flag = false;
        LOG_DEBUG("pwd error!");
      }
    } else {  // 不是登录状态,即注册,这种情况下在数据库里找到了用户名,说明用户名已被使用
      flag = false;
      LOG_DEBUG("user used!");
    }
  }
  mysql_free_result(res);

  if (!isLogin && flag == true) {  // 允许注册,进行注册
    LOG_DEBUG("regirster!");
    bzero(order, 256);
    snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(),
             pwd.c_str());
    LOG_DEBUG("%s", order);
    if (mysql_query(sql, order)) {  // 写入失败
      LOG_DEBUG("Insert error!");
      flag = false;
    }
    flag = true;
  }
  LOG_DEBUG("UserVerify success!!");
  return flag;
}

std::string HttpRequest::path() const {
  return path_;
}

std::string& HttpRequest::path() {
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
  if (post_.count(key) == 1) {
    return post_.find(key)->second;
  }
  return "";
}

std::string HttpRequest::GetPost(const char* key) const {
  assert(key != nullptr);
  if (post_.count(key) == 1) {
    return post_.find(key)->second;
  }
  return "";
}
