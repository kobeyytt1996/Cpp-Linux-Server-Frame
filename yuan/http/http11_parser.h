
#ifndef http11_parser_h
#define http11_parser_h
/**
 * 利用ragel来完成http文本的解析。ragel是有状态机实现的，有自己的语言，性能很高，甚至比汇编还快：https://blog.csdn.net/weixin_43798887/article/details/116100949
 * 这里用的是mongrel2库中对http解析的封装，底层就是用的ragel，并使用大量正则表达式：https://github.com/mongrel2/mongrel2/tree/master/src/http11
 * 把它的头文件和rl文件拷贝过来，那些.rl.cc文件都是rl文件生成出来的。这样命名来和我们自己写的源码区分
 * 安装ragel后，运行ragel -G2 -C rl文件名 -o 生成的源代码文件名
 */

/**
 * 这个头文件给服务端解析请求使用
 */
#include "http11_common.h"

typedef struct http_parser { 
  int cs;
  size_t body_start;
  int content_len;
  size_t nread;
  size_t mark;
  size_t field_start;
  size_t field_len;
  size_t query_start;
  int xml_sent;
  int json_sent;

  void *data;

  int uri_relaxed;
  field_cb http_field;
  element_cb request_method;
  element_cb request_uri;
  element_cb fragment;
  element_cb request_path;
  element_cb query_string;
  element_cb http_version;
  element_cb header_done;
  
} http_parser;

int http_parser_init(http_parser *parser);
int http_parser_finish(http_parser *parser);
size_t http_parser_execute(http_parser *parser, const char *data, size_t len, size_t off);
int http_parser_has_error(http_parser *parser);
int http_parser_is_finished(http_parser *parser);

#define http_parser_nread(parser) (parser)->nread 

#endif