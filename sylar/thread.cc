#include"thread.h"
#include"util.h"
#include"log.h"
#include<iostream>

namespace sylar{
	//要拿到当前线程需要定义一个线程局部变量，指向当前线程
	static thread_local Thread* t_thread = nullptr;
	//当前线程名称
	static thread_local std::string  t_thread_name  = "UNKNOW";

	static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

	Semaphore::Semaphore(uint32_t count){
		if(sem_init(&m_semaphore,0,count)){
			throw std::logic_error("sem_init error");
		}
	}
	Semaphore::~Semaphore(){
		sem_destroy(&m_semaphore);
	}

	void Semaphore:: wait(){
		if(sem_wait(&m_semaphore)){
			throw std::logic_error("sem_wait error");
		}
	}

	void Semaphore::notify(){
		if(sem_post(&m_semaphore)){
			throw std::logic_error("sem_post error");
		}
	}

	Thread* Thread::GetThis(){
		return t_thread;
	}

	const std::string& Thread::GetName(){
		return t_thread_name;
	}

	void Thread::SetName(const std::string& name){
		if(name.empty()) {
			return;
		}
		if(t_thread){
			t_thread->m_name = name;
		}
		t_thread_name = name;
	}

	Thread::Thread(std::function<void()> cb,const std::string& name):
		m_cb(cb),
		m_name(name){
			if(name.empty()){
				m_name = "UNKNOW";
			}
			int rt = pthread_create(&m_thread,nullptr,&Thread::run,this);
			if(rt){
				SYLAR_LOG_ERROR(g_logger)<<"pthread_create thread fail,rt="<<rt<<"name="<<name;
				throw std::logic_error("pthread_create error");	
			}
			//线程创建好，但需要等待初始化完成
			m_semaphore.wait();	
		}

	Thread::~Thread(){
		if(m_thread){

			pthread_detach(m_thread);
		}
	}

	void Thread::join(){
		if(m_thread){
			int rt = pthread_join(m_thread,nullptr);
			if(rt){
				SYLAR_LOG_ERROR(g_logger)<<"pthread_join thread fail,rt="<<rt<<"name="<<m_name;
				throw std::logic_error("pthread_join error");	
			}	
		}
		m_thread = 0;
	}

	void* Thread::run(void* arg){
		//进入线程
		Thread* thread = (Thread*)arg;
		//设置局部静态变量
		t_thread = thread;
		t_thread_name = thread->m_name;
		thread->m_id = sylar::GetThreadId();
		//给线程命名
		pthread_setname_np(pthread_self(),thread->m_name.substr(0,15).c_str());

		std::function<void()> cb;
		//与线程里面的参数 swap 防止函数内部的智能指针不会被释放
		cb.swap(thread->m_cb);
		//等待初始化完成再将线程唤醒
		thread->m_semaphore.notify();
		//线程启动
		cb();
		return 0;
	}
}
