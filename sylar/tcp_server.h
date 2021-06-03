#pragma once
#include<memory>
#include<functional>
#include"iomanager.h"
#include"address.h"
#include"socket.h"
#include"noncopyable.h"

namespace sylar{
    class TcpServer:public std::enable_shared_from_this<TcpServer>
    ,Noncopyable{
        public:
            typedef std::shared_ptr<TcpServer> ptr;
            TcpServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
                      sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
            virtual ~TcpServer();

            virtual bool bind(sylar::Address::ptr addr);
            virtual bool bind(const std::vector<sylar::Address::ptr>& addrs,std::vector<sylar::Address::ptr>& fails);
            virtual bool start();
            virtual void stop();
           
            uint64_t  getReadTimeout()const {return m_readTimeout;}
            std::string  getName()const {return m_name;}
            void setReadTimeout(uint64_t v){m_readTimeout = v;}
            void setName(const std::string& v){m_name = v;}
            bool isStop()const {return m_isStop;}
 
        protected:
            //回调
            virtual void handleClient(Socket::ptr client);
            virtual void startAccept(Socket::ptr sock);
        private:
            //socket智能指针数组，方便同时监听多地址
            std::vector<Socket::ptr> m_socks;
            //线程池
            IOManager* m_worker;
            IOManager* m_acceptWorker;
            //读超时
            uint64_t m_readTimeout;
            //方便区分和调试
            std::string m_name;
            //是否停止
            bool m_isStop;
    };
}
