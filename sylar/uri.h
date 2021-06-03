#pragma once

#include<memory>
#include<string>
#include<stdint.h>
#include"address.h"

namespace sylar{
    class Uri{
        public:
            typedef std::shared_ptr<Uri> ptr;
            Uri();
            //类的对象是字符串解析之后创建的 用静态方法 
            static Uri::ptr Create(const std::string& uri);

            //set get方法
            const std::string& getScheme() const { return m_scheme;}
            const std::string& getUserinfo() const { return m_userinfo;}
            const std::string& getHost() const { return m_host;}
            const std::string& getPath() const; 
            //{ return m_path;}
            const std::string& getQuery() const { return m_query;}
            const std::string& getFragment() const { return m_fragment;}
            int32_t getPort()const;
            //{return m_port;}

            void setScheme(const std::string& v){m_scheme = v;}
            void setUserinfo(const std::string& v){m_userinfo = v;}
            void setHost(const std::string& v){m_host = v;}
            void setPath(const std::string& v){m_path = v;}
            void setQuery(const std::string& v){m_query = v;}
            void setFragment(const std::string& v){m_fragment = v;}
            void setPort(int32_t v){m_port = v;}
            
            //流式方法和转字符串
            std::ostream& dump(std::ostream& os)const;
            std::string toString()const;
            
            //通过uri能获取一个地址
            Address::ptr createAddress()const;
        private:
            bool isDefaultPort()const;
        private:
            std::string m_scheme;
            std::string m_userinfo;
            std::string m_host;
            std::string m_path;
            std::string m_query;
            std::string m_fragment;
            int32_t m_port;
    };
}
