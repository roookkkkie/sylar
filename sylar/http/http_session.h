#pragma once

#include"sylar/socket_stream.h"
#include"http.h"
#include"http_parser.h"

namespace sylar{
    namespace http{
        class HttpSession:public SocketStream{
            public:
                typedef std::shared_ptr<HttpSession> ptr;
                HttpSession(Socket::ptr sock,bool owner = true);
                //获取httprequest 结构体
                HttpRequest::ptr recvRequest();
                //发送响应
                int sendResponse(HttpResponse::ptr rsp);
            private:

        };
    }

}
