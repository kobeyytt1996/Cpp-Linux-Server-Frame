#ifndef __YUAN_HTTP_PARSER_H__
#define __YUAN_HTTP_PARSER_H__

/**
 * 利用ragel来完成http文本的解析。ragel是有状态机实现的，有自己的语言，性能很高，媲美汇编：https://blog.csdn.net/weixin_43798887/article/details/116100949
 * 这里用的是mongrel2库中对http解析的封装，底层就是用的ragel，并使用大量正则表达式：https://github.com/mongrel2/mongrel2/tree/master/src/http11
 * 把它的头文件和rl文件拷贝过来，那些.rl.cc文件都是rl文件生成出来的。这样命名来和我们自己写的源码区分
 * 安装ragel后，运行：ragel -G2 -C rl文件名 -o 生成的源代码文件名
 * 
 * 这个头文件，是利用mongrel2库中对http解析的封装，再次封装成自己的http请求和响应的类
 * 
 * 注意：mongrel2库对http的解析不支持分段传入http报文，因此需要相应的修改两个rl文件
 */

#include "http.h"

#include "http11_parser.h"
#include "httpclient_parser.h"

namespace yuan {
namespace http {

// 以下两个是解析类
// 设计：虽然它们有一些公共的函数，但没有抽出一个基类，因为解析过程还是有些不一样
class HttpRequestParser {
public:
  typedef std::shared_ptr<HttpRequestParser> ptr;
  HttpRequestParser();

// 开始解析，实际调用http_parser_execute。不是只调用一次就能解析完，因为http的完整消息需要分多次从内核缓冲区里读到，因此会执行多次execute
// 只会解析请求行和请求头，请求体需要自己取出。执行后，data会指向未解析的部分
  size_t execute(char *data, size_t len); 
  // 判断是否已经解析完成。调用http_parser_is_finished来完成
  int isFinished();
  int hasError();

  HttpRequest::ptr getData() const { return m_data; }
  void setError(int error) { m_error = error; }

  // 获取解析出来的头部里带的Content-Length的值。有了长度，请求体就可以直接获得
  uint64_t getContentLength();

  const http_parser &getParser() { return m_parser; }
public:
  static uint64_t GetHttpRequestBufferSize();
  static uint64_t GetHttpRequestMaxBodySize();

private:
  // http_parser是利用ragel解析http请求后的结果的结构体
  http_parser m_parser;
  HttpRequest::ptr m_data;
  // 1000: invalid method; 1001: invalid version; 1002: invalid field
  int m_error;
};

class HttpResponseParser {
public:
  typedef std::shared_ptr<HttpResponseParser> ptr;
  HttpResponseParser();

  // 开始执行，实际调用http_parser_execute。不是只调用一次就能解析完，因为http的完整消息需要分多次从内核缓冲区里读到，因此会执行多次execute
  // 只会解析响应行和响应头，响应体需要自己取出。执行后，data会指向未解析的部分
  size_t execute(char *data, size_t len); 
  // 判断是否已经解析完成。调用http_parser_is_finished来完成
  int isFinished();
  int hasError();

  HttpResponse::ptr getData() const { return m_data; }
  void setError(int error) { m_error = error; }

  // 获取解析出来的头部里带的Content-Length的值。有了长度，请求体就可以直接获得
  uint64_t getContentLength();

  const httpclient_parser &getParser() { return m_parser; }
public:
  static uint64_t GetHttpResponseBufferSize();
  static uint64_t GetHttpResponseMaxBodySize();

private:
  // httpclient_parser是利用ragel解析http响应后的结果的结构体
  httpclient_parser m_parser;
  HttpResponse::ptr m_data;
  // 1001: invalid version; 1002: invalid field
  int m_error;
private:
};

}
}

#endif