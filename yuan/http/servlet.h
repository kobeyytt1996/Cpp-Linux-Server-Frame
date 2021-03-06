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

// 可以通过继承Servlet或者用下面的通用实现来给出Servlet的具体实现。e.g.比如以后要实现下载和上传功能，则单独实现两个servlet
class Servlet {
public:
    typedef std::shared_ptr<Servlet> ptr;
    Servlet(const std::string &name) : m_name(name) {}

    virtual ~Servlet() {}
    // 核心的处理方法。注意handle里不做发送响应，因为保持灵活性，可能有多层servlet嵌套。类似Java AOP思想，handle前后可能想插入一些工作
    // 传入session只是为了有上下文，比如cookie等信息
    virtual int32_t handle(HttpRequest::ptr req, HttpResponse::ptr resp, HttpSession::ptr session) = 0;

    const std::string getName() const { return m_name; }
private:
    std::string m_name;
};

// Servlet的通用实现。提供一个简单的构建Servlet的方式，可以直接传入回调函数来构建
class FunctionServlet : public Servlet {
public:
    typedef std::shared_ptr<FunctionServlet> ptr;
    typedef std::function<int32_t(HttpRequest::ptr, HttpResponse::ptr, HttpSession::ptr)> callback;

    FunctionServlet(callback cb, const std::string &name = "FunctionServlet");
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

    ServletDispatch(const std::string &name = "ServletDispatch");
    // 查找到符合req里path的Servlet，交给它来执行
    int32_t handle(HttpRequest::ptr req, HttpResponse::ptr resp, HttpSession::ptr session) override;

    void addServlet(const std::string &uri, Servlet::ptr slt);
    void addServlet(const std::string &uri, FunctionServlet::callback cb);
    void addGlobServlet(const std::string &uri, Servlet::ptr slt);
    void addGlobServlet(const std::string &uri, FunctionServlet::callback cb);

    void delServlet(const std::string &uri);
    void delGlobServlet(const std::string &uri);

    // 获取精确匹配的servlet
    Servlet::ptr getServlet(const std::string &uri);
    // 获取模糊匹配的servlet
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
    // uri(e.g. /yuan/*) -> servlet  (模糊匹配)。优先选择上面的精准匹配。TODO：这里用关联式容器是不更好？
    // 模糊匹配还可以有更多形式，如以html结尾，则返回对应页面，以php结尾，用php文件请求相应的server，再把内容返回给前端
    std::vector<std::pair<std::string, Servlet::ptr>> m_globs;
    // 所有路径都没有匹配到时再使用。初始值一般设为404错误
    Servlet::ptr m_default;
};

// 默认的找不到资源的servlet，404错误。补充：通常大型网站404会重定向url，所以浏览器显示不同的url
class NotFoundServlet : public Servlet {
public:
    typedef std::shared_ptr<NotFoundServlet> ptr;

    NotFoundServlet();
    int32_t handle(HttpRequest::ptr req, HttpResponse::ptr resp, HttpSession::ptr session) override;
};

}
}

#endif