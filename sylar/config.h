#ifndef __SYLAR_CONFIG_H_
#define __SYLAR_CONFIG_H_

#include<memory>
#include<sstream>
#include<boost/lexical_cast.hpp>

namespace sylar{
	
	class ConfigVarBase{
		public:
			typedef std::shared_ptr<ConfigVarBase> ptr;
			ConfigVarBase(const std::string& name,const std::string description=""):
			m_name(name),
			m_description(description){
			}
			virtual ~ConfigVarBase(){}
			const string::getName()const{return m_name;}
			const string::getDescription()const{ return m_description;}
			virtual std::string toString() = 0;
			virtual bool fromString(const std::string&val) = 0;
		protected:
			std::string m_name;
			std::string m_description;
	};
	
	template<class T>
		class ConfigVar::public ConfigVarBase{
			public:
				typedef std::shared_ptr<ConfigVar> ptr;

				ConfigVar(const std::string &name,const std::string description="",const T& default_value):
					ConfigVarBase(name,description),
					m_val(default_value){
					}
				std::string toString()override{
					try{
						boost::lexical_cast<std::string>(m_val);
					}catch(std::exception& e){
						SYLAR_LOG_ERROR(SYLAR_LOG_ROOT())<<"ConfigVar::toString exception"<<e.what()<<" convert: "<<typeid(m_val).name()<<" to sting";					
					}
					return "";
				}

				bool fromString(const std::string val) override{
					try{
						m_val = boost::lexical_cast<T>(val);
					}catch(std::exception& e){
						SYLAR_LOG_ERROR(SYLAR_LOG_ROOT())<<"ConfigVar::toString exception"<<e.what()<<" convert: sting to "<<typeid(m_val).name();					
					}
					return false;
				}
			private:
				T m_val;
		};
	class Config{
		public：
			typedef std::map<std::string,ConfigVarBase::ptr> ConfigVarMap;
			template<class T>
				static typename ConfigVar<T>::ptr Lookup(const std::string name,
						const T& default_value,const std::string& description =""){
					auto tmp = Lookup<T>(name);
					if(tmp){
						SYLAR_LOG_INFO(SYLAR_LOG_ROOT())<<"Lookup name = "<<name<<" exits";
						return tmp;
					}
					if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._12345678")!=std::string::npos){
						SYLAR_LOG_INFO(SYLAR_LOG_ROOT())<<"Lookup name invalid "<<name;
						throw std::invalid_argument(name);
					}
					typename ConfigVar<T>::ptr v(new ConfigVar<T>(name,description,default_value));
					s_datas(name) = v;
				}
			template<class T>
				static typename ConfigVar<T>::ptr Lookup(const std::string name){
					auto it = m_datas.find(name);
					if(it==m.datas.end()){
						return nullptr;
					}
					return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
				}
		private:
			static ConfigVarMap m_datas;
	};
}

#endif
