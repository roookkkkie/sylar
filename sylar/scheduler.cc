#include"scheduler.h"
#include"log.h"

namespace sylar{
	static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

	static thread_local Scheduler* t_scheduler = nullptr;
	static thread_local Fiber* t_fiber = nullptr;


	Scheduler::Scheduler(size_t threads,bool use_caller,const std::string name):m_name(name){
		SYLAR_ASSERT(threads>0);
		if(use_caller){
			sylar::Fiber::GetThis();
			--threads;
			//防止线程里再出现一个协程调度器？
			SYLAR_ASSERT(GetThis()==nullptr);
			t_scheduler = this;
			//新线程的主协程不参与调度，做一个新的线程去调度
			m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run,this),0,true));
			sylar::Thread::SetName(m_name);
			//线程里面声明一个调度器，并将当前线程放入调度器中的时候，
			//主协程不再是线程的主协程，而是执行run方法的主协程
			t_fiber = m_rootFiber.get();
			m_rootThread = sylar::GetThreadId();
			m_threadIds.push_back(m_rootThread);
		}else{
			m_rootThread = -1;	
		}
		m_threadCount = threads;
	}

	Scheduler:: ~Scheduler(){
		SYLAR_ASSERT(m_stopping);
		if(GetThis()==this){
			t_scheduler = nullptr;
		}
	}

	Scheduler* Scheduler::GetThis(){
		return t_scheduler;
	}

	Fiber* Scheduler::GetMainFiber(){
		return t_fiber;
	}

	//核心方法
	void Scheduler::start(){
		MutexType::Lock lock(m_mutex);
		if(!m_stopping){//false 启动
			return;
		}
		m_stopping = false;
		SYLAR_ASSERT(m_threads.empty());

		m_threads.resize(m_threadCount);
		for(size_t i = 0;i<m_threadCount;++i){
			m_threads[i].reset(new Thread(std::bind(&Scheduler::run,this),
						m_name+"_"+std::to_string(i)));
			m_threadIds.push_back(m_threads[i]->getId());
		}
		lock.unlock();//callback的时候还会上锁，所以需要在这里解锁，否则会造成死锁
		if(m_rootFiber){
		//	m_rootFiber->swapIn();
			m_rootFiber->call();
			SYLAR_LOG_INFO(g_logger)<<"call out  "<< m_rootFiber->getState();
		}
	}

	//优雅实现任务完成后退出
	void Scheduler::stop(){
		m_autoStop = true;
		//rootfiber 创建scheduler的线程的协程的执行run的协程
		if(m_rootFiber
				&& m_threadCount==0 
				&& ((m_rootFiber->getState()==Fiber::TERM)
					|| m_rootFiber->getState()==Fiber::INIT)){
			SYLAR_LOG_INFO(g_logger)<<this<<" stopped";
			m_stopping = true;

			if(stopping()){
				return;
			}
		}

		//bool exitt_on_this_fiber = false;
		if(m_rootThread != -1){
			//user_caller的线程
			SYLAR_ASSERT(GetThis() == this);
		}else{
			SYLAR_ASSERT(GetThis()!=this);
		}
		m_stopping = true;

		for(size_t i =0;i<m_threadCount;++i){
			tickle();//唤醒线程使其结束
		}
		if(m_rootFiber){
			tickle();
		}

		if(stopping()){
			return;
		}
		//if(exit_on_this_fiber){
		//
		//		}

	}

	void Scheduler::setThis(){
		t_scheduler = this;
	}

	//核心方法
	//新创建的线程直接在run里面启动，创建时被绑在run
	//users_caller的线程在run里面执行，线程池创建的线程也在run里面执行
	void Scheduler::run(){
		SYLAR_LOG_INFO(g_logger)<<"run";
		//return;
		//Fiber::GetThis();//初始化一个线程的协程
		setThis(); //将当前线程的scheduler置为自己	
		if(sylar::GetThreadId()!=m_rootThread){//线程Id不为主线程Id
			t_fiber = Fiber::GetThis().get(); //置为主线程的fiber
		}
		//创建一个idle线程，当所有调度都完成后去执行idle
		Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle,this)));		
		Fiber::ptr cb_fiber;

		FiberAndThread ft;
		while(true){
			ft.reset();
			bool tickle_me = false;
			//从消息队列中取出指定要执行的消息
			{
				MutexType::Lock lock(m_mutex);
				auto it = m_fibers.begin();
				while(it!=m_fibers.end()){
					//如果任务指定了执行线程，无需处理
					if(it->thread!=-1		
							&& it->thread!=sylar::GetThreadId()){
						++it;	
						tickle_me = true;
						continue;
					}
					//如果抢到信号的协程不为空且仍在处理，也无需处理
					SYLAR_ASSERT(it->fiber||it->cb);
					if(it->fiber&& it->fiber->getState() == Fiber::EXEC){
						++it;
						continue;
					}
					//处理
					ft = *it;
					//tickle_me = true;
					m_fibers.erase(it);
					break;
				}
			}
			if(tickle_me){
				tickle();
			}
			//执行
			//如果是fiber来做的
			if(ft.fiber&& (ft.fiber->getState()!=Fiber::TERM
						|| ft.fiber->getState()!=Fiber::EXCEPT)){
				++m_activeThreadCount;
				ft.fiber->swapIn();
				--m_activeThreadCount;

				if(ft.fiber->getState()==Fiber::READY){
					//如果是READY 则执行了YeildToReady，继续把它放到消息队列
					schedule(ft.fiber);
				}else if(ft.fiber->getState()!=Fiber::TERM
						&& ft.fiber->getState()!=Fiber::EXCEPT){
					//让出执行时间，变成hold状态
					ft.fiber->m_state = Fiber::HOLD;
				}
				ft.reset();
				//如果是回调方法来做的
			}else if(ft.cb){
				if(cb_fiber){//如果回调方法的fiber已经创建好了
					//fiber reset交换cb
					cb_fiber->reset(ft.cb);
				}else{
					cb_fiber.reset(new Fiber(ft.cb));
				}

				ft.reset();

				++m_activeThreadCount;
				cb_fiber->swapIn();
				--m_activeThreadCount;

				//如果从协程回来执行是ready
				if(cb_fiber->getState()==Fiber::READY){
					schedule(cb_fiber);
					cb_fiber.reset();//智能指针的reset
				}else if(cb_fiber->getState()==Fiber::EXCEPT
						|| cb_fiber->getState()==Fiber::TERM){
					cb_fiber->reset(nullptr);//智能指针的reset
				}else {//if(cb_fiber->getState()!=Fiber::TERM){
					cb_fiber->m_state = Fiber::HOLD;
					cb_fiber.reset();
				}	
				//没任务做，执行idle的fiber
				}else{//直到没有任务执行
					if(idle_fiber->getState() == Fiber::TERM){
						SYLAR_LOG_INFO(g_logger)<<"idle fiber term";
						//idle_fiber.reset();
						break;//从while true退出，整个任务完成
						//continue;
					}
					//如果ider在执行
					++m_idleThreadCount;
					idle_fiber->swapIn();
					--m_idleThreadCount;
					//回来之后的状态不为结束，将其置为hold
					if(idle_fiber->getState() != Fiber::TERM//滑铁卢，状态只能为1种
							&& idle_fiber->getState()!= Fiber::EXCEPT){//所以这里用||为true
						//应该是不为这两者
						idle_fiber->m_state = Fiber::HOLD;
					}
				}
			}
		}

		void Scheduler::tickle(){
			SYLAR_LOG_INFO(g_logger)<<"tickle";
		}

		bool Scheduler::stopping(){
			MutexType::Lock lock(m_mutex);
			return m_autoStop&&m_stopping
			   &&m_fibers.empty()&&m_activeThreadCount == 0;	
		}

		void Scheduler::idle(){
			SYLAR_LOG_INFO(g_logger)<<"idle";
		}
	}

