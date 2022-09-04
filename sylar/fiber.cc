#include"fiber.h"
#include<atomic>
#include"config.h"
#include"log.h"
#include"macro.h"
#include"scheduler.h"

namespace sylar{
	static Logger::ptr g_logger = SYLAR_LOG_NAME("system");
	//全局的原子量来计数
	static std::atomic<uint64_t> s_fiber_id {0};
	static std::atomic<uint64_t> s_fiber_count {0};

	//线程局部变量 表示这个就是当前线程的主协程
	static thread_local Fiber* t_fiber = nullptr;
	//类似main协程
	static thread_local Fiber::ptr t_threadFiber = nullptr;

	//设置fiber_size
	static ConfigVar<uint32_t>::ptr g_fiber_stack_size = 
		Config::Lookup<uint32_t>("fiber.stack_size",1024*1024,"fiber stack size");

	//内存分配器用于测试内存分配的效率
	class MallocStackAllocator{
		public:
			static void* Alloc(size_t size){
				return malloc(size);
			}
			//在dealloc的时候使用size是为了兼容后续可能扩展的mmap和umap
			static void Dealloc(void* vp,size_t size){
				return free(vp);
			}
	};
	//方便修改，如果想更换分配方式，只需在此处修改
	using StackAllocator = MallocStackAllocator;

	uint64_t Fiber::GetFiberId(){
		//不能直接用GetThis，因为有些线程是没有协程的
		//如果用GetThis,这些线程就会初始化为主协程
		if(t_fiber){
			//当协程存在的时候
			return t_fiber->getId();
		}
		return 0;
	}

	//将线程的上下文赋给协程
	Fiber::Fiber(){//私有的构造函数,获取当前线程的上下文
		m_state = EXEC;
		SetThis(this);

		//协程获取当前线程的上下文，main协程
		if(getcontext(&m_ctx)){
			SYLAR_ASSERT2(false,"getcontext");
		}

		++s_fiber_count;
		SYLAR_LOG_DEBUG(g_logger)<<"Fiber::Fiber main";
	}

	//真正创建协程
	Fiber::Fiber(std::function<void()>cb,size_t stacksize,bool use_caller):
		m_id(++s_fiber_id),
		m_cb(cb){
			++s_fiber_count;
			m_stacksize = stacksize?stacksize:g_fiber_stack_size->getValue();

			m_stack = StackAllocator::Alloc(m_stacksize);
			//协程获取上下文
			if(getcontext(&m_ctx)){
				SYLAR_ASSERT2(false,"getcontext");
			}
			//生成上下文关联
			m_ctx.uc_link = nullptr;
			m_ctx.uc_stack.ss_sp = m_stack;
			m_ctx.uc_stack.ss_size = m_stacksize;
			if(!use_caller){
				//将当前上下文设置好，将Mainfunc关联进去,swapcontext时会调用manfunc
				makecontext(&m_ctx,&Fiber::MainFunc,0);
			}else{
				makecontext(&m_ctx,&Fiber::CallerMainFunc,0);	
			}

			SYLAR_LOG_DEBUG(g_logger)<<"Fiber::Fiber id = "<<m_id;
		}

	Fiber::~Fiber(){
		--s_fiber_count;
		//存在栈，是创建出来的协程
		if(m_stack){
			SYLAR_ASSERT(m_state == TERM
					|| m_state == EXCEPT
					|| m_state ==INIT);
			StackAllocator::Dealloc(m_stack,m_stacksize);
		}
		else{ //是主协程(当前线程）
			SYLAR_ASSERT(!m_cb);
			SYLAR_ASSERT(m_state == EXEC);

			Fiber* cur = t_fiber;
			if(cur == this){
				SetThis(nullptr);
			}
		}
		SYLAR_LOG_DEBUG(g_logger)<<"Fiber::~Fiber id = "<<m_id;
	}

	//重置协程函数并重置状态 INIT，TERN,利用内存
	void Fiber::reset(std::function<void()> cb){
		SYLAR_ASSERT(m_stack);
		SYLAR_ASSERT(m_state == TERM
				|| m_state == EXCEPT
				|| m_state == INIT);
		m_cb = cb;
		if(getcontext(&m_ctx)){
			SYLAR_ASSERT2(false,"getcontext");
		}
		m_ctx.uc_link = nullptr;
		m_ctx.uc_stack.ss_sp = m_stack;
		m_ctx.uc_stack.ss_size = m_stacksize;

		makecontext(&m_ctx,&Fiber::MainFunc,0);
		m_state = INIT;
	}
	//切换到当前协程执行(当前操作的协程和当前运行的协程）
	void Fiber::swapIn(){
		SetThis(this);
		SYLAR_ASSERT(m_state != EXEC);
		m_state = EXEC;
		//协程的交换操作	
		//	if(swapcontext(&t_threadFiber->m_ctx,&m_ctx)){//与当前线程的交换操作
		//	变成与主协程交换上下文
		if(swapcontext(&Scheduler::GetMainFiber()->m_ctx,&m_ctx)){
			SYLAR_ASSERT2(false,"swapcontext");	
		}
	}

	void Fiber::call(){
		SetThis(this);
		m_state = EXEC;
		//	SYLAR_ASSERT(GetThis() == t_threadFiber);
		SYLAR_LOG_ERROR(g_logger)<<getId();
		if(swapcontext(&t_threadFiber->m_ctx,&m_ctx)){
			SYLAR_ASSERT2(false,"swapcontext");		
		}
	}

	void Fiber::back(){
		SetThis(t_threadFiber.get());
		if(swapcontext(&m_ctx,&t_threadFiber->m_ctx)){
			SYLAR_ASSERT2(false,"swapcontext");	
		}	
	}

	//切换到后台执行
	void Fiber::swapOut(){
		//SetThis(t_threadFiber.get());
		//	if(t_fiber != Scheduler::GetMainFiber()){//当前执行的协程不是主协程，不是thhs
		SetThis(Scheduler::GetMainFiber());
		//if(swapcontext(&m_ctx,&t_threadFiber->m_ctx)){
		if(swapcontext(&m_ctx,&Scheduler::GetMainFiber()->m_ctx)){	
			SYLAR_ASSERT2(false,"swapcontext");	
		}
		/*	
			}else{
			SetThis(t_threadFiber.get());
			if(swapcontext(&m_ctx,&t_threadFiber->m_ctx)){
			SYLAR_ASSERT2(false,"swapcontext");	
			}	
			}
			*/
	}
	//设置当前协程
	void Fiber::SetThis(Fiber* f){
		t_fiber = f;
	}
	//返回当前协程,如果没有,在线程上新建一个主协程
	Fiber::ptr Fiber::GetThis(){
		if(t_fiber){
			//返回到协程的智能指针
			return t_fiber->shared_from_this();
		}
		Fiber::ptr main_fiber(new Fiber);
		SYLAR_ASSERT(t_fiber == main_fiber.get());
		t_threadFiber = main_fiber;
		return t_fiber->shared_from_this();
	}
	//协程切换到后台并设置为Ready状态
	void Fiber::YieldToReady(){
		//将当前执行的协程切换为ready状态，并切换回主线程
		Fiber::ptr cur = GetThis();
    SYLAR_ASSERT(cur->m_state == EXEC);
		cur->m_state = READY;
		cur->swapOut();
	}
	//协程切换到后台并设置为Hold状态
	void Fiber::YieldToHold(){
		//将当前执行的协程切换为hold状态，并切换回主线程
		Fiber::ptr cur = GetThis();
    SYLAR_ASSERT(cur->m_state == EXEC);
	//	cur->m_state = HOLD;
		cur->swapOut();	
	}
	//总协程数
	uint64_t Fiber::TotalFibers(){
		return s_fiber_count;
	}
	//协程执行自己的cb
	void Fiber:: MainFunc(){
		Fiber::ptr cur = GetThis();
		SYLAR_ASSERT(cur);
		try{
			cur->m_cb();
			cur->m_cb = nullptr;
			cur->m_state = TERM;
		}catch(std::exception& ex){
			cur->m_state = EXCEPT;
			SYLAR_LOG_ERROR(g_logger)<<"Fiber Except: "<<ex.what()
				<<" fiber_id = "<<cur->getId()
				<<std::endl
				<<sylar::BacktraceToString();
		}catch(...){
			cur->m_state = EXCEPT;
			SYLAR_LOG_ERROR(g_logger)<<"Fiber Except: "
				<<" fiber_id = "<<cur->getId()
				<<std::endl
				<<sylar::BacktraceToString();
		}
		//程序执行完毕，返回主协程
		auto raw_ptr = cur.get();
		cur.reset();//减少引用计数
		//默认切换到线程的主协程上面
		raw_ptr->swapOut();//从这里swapOut出去，下一句断言不可达
		//协程的析构析构了整个栈，不存在内存泄漏
		SYLAR_ASSERT2(false,"never reach fiber_id = "+std::to_string(raw_ptr->getId()));
	}

	void Fiber::CallerMainFunc(){
		Fiber::ptr cur = GetThis();
		SYLAR_ASSERT(cur);
		try{
			cur->m_cb();
			cur->m_cb = nullptr;
			cur->m_state = TERM;
		}catch(std::exception& ex){
			cur->m_state = EXCEPT;
			SYLAR_LOG_ERROR(g_logger)<<"Fiber Except: "<<ex.what()
				<<" fiber_id = "<<cur->getId()
				<<std::endl
				<<sylar::BacktraceToString();
		}catch(...){
			cur->m_state = EXCEPT;
			SYLAR_LOG_ERROR(g_logger)<<"Fiber Except: "
				<<" fiber_id = "<<cur->getId()
				<<std::endl
				<<sylar::BacktraceToString();
		}
		//程序执行完毕，返回主协程
		auto raw_ptr = cur.get();
		cur.reset();//减少引用计数
		//默认切换到线程的主协程上面
		raw_ptr->back();//从这里swapOut出去，下一句断言不可达
		//协程的析构析构了整个栈，不存在内存泄漏
		SYLAR_ASSERT2(false,"never reach fiber_id = "+std::to_string(raw_ptr->getId()));
	}
	}
