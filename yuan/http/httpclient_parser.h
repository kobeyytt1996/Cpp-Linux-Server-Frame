/**
 *
 * Copyright (c) 2010, Zed A. Shaw and Mongrel2 Project Contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 *     * Neither the name of the Mongrel2 Project, Zed A. Shaw, nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef httpclient_parser_h
#define httpclient_parser_h

/**
 * 利用ragel来完成http文本的解析。ragel是有状态机实现的，有自己的语言，性能很高，甚至比汇编还快：https://blog.csdn.net/weixin_43798887/article/details/116100949
 * 这里用的是mongrel2库中对http解析的封装，底层就是用的ragel，并使用大量正则表达式：https://github.com/mongrel2/mongrel2/tree/master/src/http11
 * 把它的头文件和rl文件拷贝过来，那些.rl.cc文件都是rl文件生成出来的。这样命名来和我们自己写的源码区分
 * 安装ragel后，运行ragel -G2 -C rl文件名 -o 生成的源代码文件名
 */

/**
 * 这个头文件给客户端解析响应使用
 */

#include "http11_common.h"

typedef struct httpclient_parser { 
  int cs;
  size_t body_start;
  int content_len;
  int status;
  int chunked;
  int chunks_done;
  int close;
  size_t nread;
  size_t mark;
  size_t field_start;
  size_t field_len;

  void *data;

  field_cb http_field;
  element_cb reason_phrase;
  element_cb status_code;
  element_cb chunk_size;
  element_cb http_version;
  element_cb header_done;
  element_cb last_chunk;
  
  
} httpclient_parser;

int httpclient_parser_init(httpclient_parser *parser);
int httpclient_parser_finish(httpclient_parser *parser);
int httpclient_parser_execute(httpclient_parser *parser, const char *data, size_t len, size_t off);
int httpclient_parser_has_error(httpclient_parser *parser);
int httpclient_parser_is_finished(httpclient_parser *parser);

#define httpclient_parser_nread(parser) (parser)->nread 

#endif