#pragma once
#include"sylar/tcp_server.h"
#include"http_session.h"
#include"servlet.h"

namespace sylar{
    namespace http{
        class HttpServer:public TcpServer{
            public:
                typedef std::shared_ptr<HttpServer> ptr;
                HttpServer(bool keepalive=false,
                        sylar::IOManager* worker = sylar::IOManager::GetThis(),
                        sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
                
                //auto sd = server->getServletDispatch();

                ServletDispatch::ptr getServletDispatch()const {return m_dispatch;}
                void setServletDispath(ServletDispatch::ptr v) {m_dispatch = v;}
            protected:
                void handleClient(Socket::ptr client)override;
            private:
                bool m_isKeepAlive;
                ServletDispatch::ptr m_dispatch;
        };
    }
}
