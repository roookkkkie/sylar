#pragma once

#include"sylar/socket_stream.h"
#include"http.h"
#include"http_parser.h"

namespace sylar{
    namespace http{
        class HttpConnection:public SocketStream{
            public:
                typedef std::shared_ptr<HttpConnection> ptr;
                HttpConnection(Socket::ptr sock,bool owner = true);
                //获取httprequest 结构体
                HttpResponse::ptr recvResponse();
                //发送响应
                int sendRequest(HttpRequest::ptr req);
            private:

        };
    }

}
