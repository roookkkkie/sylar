#include"http_session.h"

namespace sylar{
    namespace http{
        HttpSession::HttpSession(Socket::ptr sock,bool owner)
            :SocketStream(sock,owner){

            }
        HttpRequest::ptr HttpSession::recvRequest(){
            HttpRequestParser::ptr parser(new HttpRequestParser);
            uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
            //uint64_t buff_size = 150;
            //防止出了作用域内存不释放
            std::shared_ptr<char> buffer(new char[buff_size],[](char*ptr){delete[]ptr;});
            //使用的时候用裸指针
            char* data = buffer.get();
            //offset记录的是还未解析的部分的起始位置
            int  offset = 0;
            do{
                int len = read(data+offset,buff_size-offset);
                if(len<=0){
                    close();
                    return nullptr;
                }
                len+=offset;
                size_t nparse = parser->execute(data,len);
                if(parser->hasError()){
                    close();
                    return nullptr;
                }
                offset = len - nparse;
                //如果缓冲区满了也没有解析
                if(offset == (int)buff_size){
                    close();
                    return nullptr;
                }
                if(parser->isFinished()){
                    break;    
                }
            }while(true);
            int64_t length = parser->getContentLength();
            if(length>0){
                std::string body;
                body.resize(length);
                int len = 0;
                if(length >= offset){
                    //向body里拷贝从data[0]起始的offset个字节
                    memcpy(&body[0],data,offset);
                    len = offset;
                }else{
                    memcpy(&body[0],data,length);
                    len = length;
                }
                length -= offset;
                if(length>0){
                    if(readFixSize(&body[len],length)<=0){
                        close();
                        return nullptr;
                    }
                }
                parser->getData()->setBody(body);
            }
            parser->getData()->init();
            return parser->getData();
        }
        int HttpSession::sendResponse(HttpResponse::ptr rsp){
            std::stringstream ss;
            ss<<*rsp;
            std::string data = ss.str();
            return writeFixSize(data.c_str(),data.size());
        }




    }
}
