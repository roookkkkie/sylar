#ifndef __SYLAR_FIBER_H__
#define __SYLAR_FIBER_H__
#include<memory>
#include<functional>
#include<ucontext.h>
#include"thread.h"

namespace sylar{
	class Scheduler;
	//获取当前类的智能指针，不能在栈上创建对象
	class Fiber:public std::enable_shared_from_this<Fiber>{
		friend class Scheduler;
		public:
		typedef std::shared_ptr<Fiber> ptr;

		enum State{
			INIT,
			HOLD,
			EXEC,
			TERM,
			READY,
			EXCEPT
		};

		private:
		Fiber();

		public:
		Fiber(std::function<void()>cb,size_t stacksize = 0,bool use_caller = false);
		~Fiber();

		uint64_t getId()const {return m_id;}

		//重置协程函数并重置状态 INIT，TERN
		void reset(std::function<void()> cb);
		//切换到当前协程执行
		void swapIn();
		//强行将当前协程置换为目标执行协程
		void call();
		void back();	
		//切换到后台执行
		void swapOut();
		State getState() const { return m_state;}
		public:
		//设置当前协程
		static void SetThis(Fiber* f);
		//返回当前协程
		static Fiber::ptr GetThis();
		//协程切换到后台并设置为Ready状态
		static void YieldToReady();
		//协程切换到后台并设置为Hold状态
		static void YieldToHold();
		//总协程数
		static uint64_t TotalFibers();

		static void MainFunc();
		static void CallerMainFunc();
		static uint64_t GetFiberId();
		private:
		uint64_t m_id = 0;
		uint32_t m_stacksize = 0;
		State m_state = INIT;
		ucontext_t m_ctx;
		void * m_stack = nullptr;

		std::function<void()> m_cb;	
	};

}

#endif
