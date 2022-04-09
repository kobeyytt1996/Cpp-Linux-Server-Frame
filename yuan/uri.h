#ifndef __YUAN_URI_H__
#define __YUAN_URI_H__
/**
 * uri解析的封装类：OOP思想。URL只是URI的一种。后续也可以用uri去实现一些服务。解析也用ragel来做。
 * 以下具体的解析在uri.rl里
 * rfc文档：https://www.rfc-editor.org/rfc/rfc3986
 *  foo://user@example.com:8042/over/there?name=ferret#nose
    \_/   \___________________/\_________/ \_________/ \__/
     |           |                   |            |       |
  scheme     authority(三部分)      path       query   fragment
 */

#include <iostream>
#include <memory>
#include <string>

#include "address.h"

namespace yuan {

class Uri {
public:
    typedef std::shared_ptr<Uri> ptr;

    // 重点：解析uri格式的String，利用ragel这个有限确定状态机。仿造http11_parser.rl来写的。都是使用ragel的标准写法
    static Uri::ptr CreateUri(const std::string &uri_str);
    Uri();

    const std::string &getScheme() const { return m_scheme; }
    const std::string &getUserinfo() const { return m_userinfo; }
    const std::string &getHost() const { return m_host; }
    const std::string &getPath() const { return m_path; }
    const std::string &getQuery() const { return m_query; }
    const std::string &getFragment() const { return m_fragment; }
    // 获取端口号要判断，如果没有显示设置端口号，要获取公知端口号
    int32_t getPort() const;

    void setScheme(const std::string &val) { m_scheme = val; }
    void setUserinfo(const std::string &val) { m_userinfo = val; }
    void setHost(const std::string &val) { m_host = val; }
    void setPath(const std::string &val) { m_path = val; }
    void setQuery(const std::string &val) { m_query = val; }
    void setFragment(const std::string &val) { m_fragment = val; }
    void setPort(int32_t val) { m_port = val; }

    std::ostream &dump(std::ostream &os) const;
    const std::string toString() const;
    // 无效则创建失败
    Address::ptr createAddress() const;
private:
    // 是否使用scheme对应的公知端口号
    bool isDefaultPort() const;
private:
    std::string m_scheme;
    std::string m_userinfo;
    std::string m_host;
    int32_t m_port = 0;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
};

std::ostream &operator<<(std::ostream &os, const Uri &uri);

}


#endif