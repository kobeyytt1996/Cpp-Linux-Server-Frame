#include "servlet.h"

namespace yuan {
namespace http {

FunctionServlet::FunctionServlet(callback cb, const std::string &name)
    : Servlet(name), m_cb(cb) {}

int32_t FunctionServlet::handle(HttpRequest::ptr req
                                , HttpResponse::ptr resp, HttpSession::ptr session) {
    return m_cb(req, resp, session);
}

ServletDispatch::ServletDispatch(const std::string &name)
    : Servlet(name), m_default(new NotFoundServlet()) {}

int32_t ServletDispatch::handle(HttpRequest::ptr req
                                , HttpResponse::ptr resp, HttpSession::ptr session) {
    return 0;
}

void ServletDispatch::addServlet(const std::string &uri, Servlet::ptr slt) {
    RWMutexType::WriteLock w_lock(m_mutex);
    m_datas[uri] = slt;
}

void ServletDispatch::addServlet(const std::string &uri, FunctionServlet::callback cb) {
    RWMutexType::WriteLock w_lock(m_mutex);
    m_datas[uri] = std::make_shared<FunctionServlet>(cb);
}

void ServletDispatch::addGlobServlet(const std::string &uri, Servlet::ptr slt) {
    RWMutexType::WriteLock w_lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            it->second = slt;
            return;
        }
    }
    m_globs.push_back({uri, slt});
}

void ServletDispatch::addGlobServlet(const std::string &uri, FunctionServlet::callback cb) {
    Servlet::ptr temp(new FunctionServlet(cb));
    addGlobServlet(uri, temp);
}

void ServletDispatch::delServlet(const std::string &uri) {
    RWMutexType::WriteLock w_lock(m_mutex);
    m_datas.erase(uri);
}

void ServletDispatch::delGlobServlet(const std::string &uri) {
    RWMutexType::WriteLock w_lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            m_globs.erase(it);
            return;
        }
    }
}

Servlet::ptr ServletDispatch::getServlet(const std::string &uri) {
    RWMutexType::ReadLock r_lock(m_mutex);
    auto it = m_datas.find(uri);
    return (it == m_datas.end()) ? nullptr : it->second;
}

Servlet::ptr ServletDispatch::getGlobServlet(const std::string &uri) {
    RWMutexType::ReadLock r_lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            return it->second;
        }
    }
    return nullptr;
}

// 精准匹配-》模糊匹配-》默认
Servlet::ptr ServletDispatch::getMatchedServlet(const std::string &uri) {
    auto it = getServlet(uri);
    if (it) {
        return it;
    }
    it = getGlobServlet(uri);
    if (it) {
        return it;
    }
    return m_default;
}

NotFoundServlet::NotFoundServlet() : Servlet("NotFoundServlet") {}

int32_t NotFoundServlet::handle(HttpRequest::ptr req
    , HttpResponse::ptr resp, HttpSession::ptr session) {
    static const std::string RESP_BODY = "<html><head><title>404 Not Found</title>"
        "</head><body><h1>Not Found</h1><p>The requested URL /hh was not found on YUAN.</p></body></html>";

    resp->setStatus(HttpStatus::NOT_FOUND);
    resp->setHeader("Content-Type", "text/html");         
    resp->setHeader("Server", "yuan/1.0.0");         
    resp->setBody(RESP_BODY);      
    return 0;   
}


}
}