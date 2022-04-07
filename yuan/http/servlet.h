#ifndef __YUAN_SERVLET_H__
#define __YUAN_SERVLET_H__

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../thread.h"
#include "http.h"
#include "http_session.h"

namespace yuan {
namespace http {

class Servlet {
public:
    typedef std::shared_ptr<Servlet> ptr;
    Servlet(const std::string &name) : m_name(name) {}

    virtual ~Servlet();
    // 核心的处理方法
    virtual int32_t handle(HttpRequest::ptr req, HttpResponse::ptr resp, HttpSession::ptr session) = 0;

    const std::string getName() const { return m_name; }
private:
    std::string m_name;
};

// Servlet的通用实现
class FunctionServlet : public Servlet {
public:
    typedef std::shared_ptr<FunctionServlet> ptr;
    typedef std::function<int32_t(HttpRequest::ptr, HttpResponse::ptr, HttpSession::ptr)> callback;

    FunctionServlet(callback cb);
    int32_t handle(HttpRequest::ptr req, HttpResponse::ptr resp, HttpSession::ptr session) override;
private:
    callback m_cb;
};

// Servlet的管理类。也有handle方法，只不过是由它决定具体请求哪个servlet
class ServletDispatch : public Servlet {
public:
    typedef std::shared_ptr<ServletDispatch> ptr;
    // 明显写少读多，故用读写锁
    typedef RWMutex RWMutexType;

    ServletDispatch();
    int32_t handle(HttpRequest::ptr req, HttpResponse::ptr resp, HttpSession::ptr session) override;
    void addServlet(const std::string &uri, Servlet::ptr slt);
    void addServlet(const std::string &uri, FunctionServlet::callback cb);
    void addGlobServlet(const std::string &uri, Servlet::ptr slt);
    void addGlobServlet(const std::string &uri, FunctionServlet::callback cb);

    void delServlet(const std::string &uri);
    void delGlobServlet(const std::string &uri);

    Servlet::ptr getServlet(const std::string &uri);
    Servlet::ptr getGlobServlet(const std::string &uri);

    // 精准匹配-》模糊匹配-》默认
    Servlet::ptr getMatchedServlet(const std::string &uri);

    Servlet::ptr getDefaultServlet() const { return m_default; }
    // TODO:有线程安全问题。以后再解决
    void setDefaultServlet(Servlet::ptr slt) { m_default = slt; }

private:
    RWMutexType m_mutex;
    // uri(e.g. /yuan/xxx) -> servlet  (精准匹配)
    std::unordered_map<std::string, Servlet::ptr> m_datas;
    // uri(e.g. /yuan/*) -> servlet  (模糊匹配)。优先选择上面的精准匹配
    std::vector<std::pair<std::string, Servlet::ptr>> m_globs;
    // 所有路径都没有匹配到时再使用
    Servlet::ptr m_default;
};

}
}

#endif