#ifndef __YUAN_URI_H__
#define __YUAN_URI_H__
/**
 * uri解析的封装类：OOP思想。URL只是URI的一种。后续也可以用uri去实现一些服务
 * rfc文档：https://www.rfc-editor.org/rfc/rfc3986
 *  foo://user@example.com:8042/over/there?name=ferret#nose
    \_/   \___________________/\_________/ \_________/ \__/
     |           |                   |            |       |
  scheme     authority(三部分)      path       query   fragment
 */

#include <memory>
#include <string>

namespace yuan {

class Uri {
public:
    typedef std::shared_ptr<Uri> ptr;
    Uri();

    const std::string &getScheme() const { return m_scheme; }
    const std::string &getUserinfo() const { return m_userinfo; }
    const std::string &getHost() const { return m_host; }
    const std::string &getPath() const { return m_path; }
    const std::string &getQuery() const { return m_query; }
    const std::string &getFragment() const { return m_fragment; }
    int32_t getPort() const { return m_port; }

    void setScheme(const std::string &val) { m_scheme = val; }
    void setUserinfo(const std::string &val) { m_userinfo = val; }
    void setHost(const std::string &val) { m_host = val; }
    void setPath(const std::string &val) { m_path = val; }
    void setQuery(const std::string &val) { m_query = val; }
    void setFragment(const std::string &val) { m_fragment = val; }
    void setPort(int32_t val) { m_port = val; }
private:
    std::string m_scheme;
    std::string m_userinfo;
    std::string m_host;
    int32_t m_port;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
};

}


#endif