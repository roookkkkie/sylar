#include"iomanager.h"
#include"macro.h"
#include"log.h"

#include<unistd.h>
#include<sys/epoll.h>
#include<string.h>
#include<fcntl.h>
#include<errno.h>

namespace sylar{

	static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

	IOManager::FdContext::EventContext& IOManager::FdContext::getContext(IOManager::Event event){
		switch(event){
			case IOManager::READ:
				return read;
			case IOManager::WRITE:
				return write;
			default:
				SYLAR_ASSERT2(false,"getContext");
		}
	}

	void IOManager::FdContext::resetContext(IOManager::FdContext::EventContext& ctx){
		ctx.scheduler = nullptr;
		ctx.fiber.reset();
		ctx.cb = nullptr;
	}

	void IOManager::FdContext::triggerEvent(IOManager::Event event){
		SYLAR_ASSERT(events & event);
		events = (Event)(events& ~event);
		EventContext& ctx = getContext(event);
		if(ctx.cb){
			ctx.scheduler->schedule(&ctx.cb);
		}else{
			ctx.scheduler->schedule(&ctx.fiber);
		}
		ctx.scheduler = nullptr;
		return;
	}


	IOManager::IOManager(size_t threads,bool use_caller,const std::string name):
		Scheduler(threads,use_caller,name){
			m_epfd = epoll_create(5000); 	
			SYLAR_ASSERT(m_epfd>0);

			int rt = pipe(m_tickleFds);
			SYLAR_ASSERT(!rt);

			epoll_event event;
			memset(&event,0,sizeof(epoll_event));
			event.events = EPOLLIN|EPOLLET;				//边缘触发，只通知一次
			event.data.fd = m_tickleFds[0];

			rt = fcntl(m_tickleFds[0],F_SETFL,O_NONBLOCK);
			SYLAR_ASSERT(!rt);

			rt = epoll_ctl(m_epfd,EPOLL_CTL_ADD,m_tickleFds[0],&event);
			SYLAR_ASSERT(!rt);

			contextResize(32);							//m_fdContext已经被初始化

			start();									//创建好就默认启动
		}

	IOManager::~IOManager(){
		stop();
		close(m_epfd);
		close(m_tickleFds[0]);
		close(m_tickleFds[1]);

		//内存释放
		for(size_t i=0;i<m_fdContexts.size();++i){
			if(m_fdContexts[i]){
				delete m_fdContexts[i];				//裸指针	
			}
		}
	}

	void IOManager::contextResize(size_t size){
		m_fdContexts.resize(size);
		for(size_t i=0;i<m_fdContexts.size();++i){
			if(!m_fdContexts[i]){
				m_fdContexts[i] = new FdContext;
				m_fdContexts[i]->fd = i;			//索引为fd
			}
		}
	}

	int IOManager::addEvent(int fd,Event event,std::function<void()>cb){
		//取出fd的上下文的类
		FdContext* fd_ctx = nullptr;
		RWMutexType::ReadLock lock(m_mutex);
		if((int)m_fdContexts.size() > fd){
			fd_ctx =m_fdContexts[fd];
			lock.unlock();
		}else{
			lock.unlock();
			RWMutexType::WriteLock lock2(m_mutex);
			contextResize(fd * 1.5);
			fd_ctx = m_fdContexts[fd];
		}

		//设置fd上下文的状态
		FdContext::MutexType::Lock lock2(fd_ctx->mutex);
		//如果连续添加同一类型的事件，是错误的
		if(fd_ctx->events & event){
			SYLAR_LOG_ERROR(g_logger)<<"addEvent assert fd = "<<fd
				<<" event = "<<event
				<<"fd_ctx->events = "<<fd_ctx->events;
			SYLAR_ASSERT(!(fd_ctx->events&event));
		}

		int op = fd_ctx->events?EPOLL_CTL_MOD:EPOLL_CTL_ADD;
		epoll_event epevent;
		epevent.events = EPOLLET|fd_ctx->events|event;			//新的event为边缘触发
		//新的结构体可以携带外带数据,之后可以拿回
		epevent.data.ptr = fd_ctx;

		int rt = epoll_ctl(m_epfd,op,fd,&epevent);
		if(rt){
			SYLAR_LOG_ERROR(g_logger)<<"epoll_ctl("<<m_epfd<<","<<op<<","<<fd<<","
				<<&epevent.events<<"): "<<rt<<" ("<<errno<<") ("<<strerror(errno)<<")";
			return -1;	
		}
		++m_pendingEventCount;
		fd_ctx->events =(Event) (fd_ctx->events|event);
		//返回为读事件或者写事件
		FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
		//注册时三者肯定为空
		SYLAR_ASSERT(!event_ctx.scheduler
				&& !event_ctx.fiber
				&& !event_ctx.cb);

		//设置
		event_ctx.scheduler = Scheduler::GetThis();				//注册该事件的当前协程调度器
		if(cb){
			event_ctx.cb.swap(cb);
		}else{
			event_ctx.fiber = Fiber::GetThis();
			SYLAR_ASSERT2(event_ctx.fiber->getState()==Fiber::EXEC,
					"state = "<< event_ctx.fiber->getState());
		}
		return 0;
	}

	//删除事件 
	bool IOManager::delEvent(int fd,Event event){	
		RWMutexType::ReadLock lock(m_mutex);
		if((int)m_fdContexts.size() <= fd){
			return false;
		}
		FdContext* fd_ctx = m_fdContexts[fd];
		lock.unlock();

		FdContext::MutexType::Lock lock2(fd_ctx->mutex);
		//如果没有这个事件，无需操作
		if(!(fd_ctx->events&event)){
			return false;
		}
		//将event从中去掉,此处就是删除操作
		Event new_events = (Event)(fd_ctx->events & ~event);
		//如果仅有该事件则删除，否则修改
		int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
		epoll_event epevent;
		epevent.events = EPOLLET | new_events;
		epevent.data.ptr = fd_ctx;
		int rt = epoll_ctl(m_epfd,op,fd,&epevent);
		if(rt){
			SYLAR_LOG_ERROR(g_logger)<<"epoll_ctl("<<m_epfd<<","<<op<<","<<fd<<","
				<<&epevent.events<<"): "<<rt<<" ("<<errno<<") ("<<strerror(errno)<<")";
			return false;	
		}
		--m_pendingEventCount;
		fd_ctx->events = new_events;
		//返回为读事件或者写事件
		FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
		fd_ctx->resetContext(event_ctx);
		return true;
	}

	//找到对应事件,强制触发执行
	bool IOManager::cancelEvent(int fd,Event event){
		RWMutexType::ReadLock lock(m_mutex);
		if((int)m_fdContexts.size() <= fd){
			return false;
		}
		FdContext* fd_ctx = m_fdContexts[fd];
		lock.unlock();

		FdContext::MutexType::Lock lock2(fd_ctx->mutex);
		//如果没有这个事件，无需操作
		if(!(fd_ctx->events&event)){
			return false;
		}
		//将event从中去掉,此处就是删除操作
		Event new_events = (Event)(fd_ctx->events & ~event);
		//如果仅有该事件则删除，否则修改
		int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
		epoll_event epevent;
		epevent.events = EPOLLET | new_events;
		epevent.data.ptr = fd_ctx;
		int rt = epoll_ctl(m_epfd,op,fd,&epevent);
		if(rt){
			SYLAR_LOG_ERROR(g_logger)<<"epoll_ctl("<<m_epfd<<","<<op<<","<<fd<<","
				<<&epevent.events<<"): "<<rt<<" ("<<errno<<") ("<<strerror(errno)<<")";
			return false;	
		}
		//FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
		//强制触发这个事件的操作
		fd_ctx->triggerEvent(event);
		--m_pendingEventCount;
		return true;
	}

	bool IOManager::cancelAll(int fd){
		RWMutexType::ReadLock lock(m_mutex);
		if((int)m_fdContexts.size() <= fd){
			return false;
		}
		FdContext* fd_ctx = m_fdContexts[fd];
		lock.unlock();

		FdContext::MutexType::Lock lock2(fd_ctx->mutex);
		//判断有没有事件
		if(!(fd_ctx->events)){
			return false;
		}
		int op = EPOLL_CTL_DEL;
		epoll_event epevent;
		epevent.events = 0;
		epevent.data.ptr = fd_ctx;
		int rt = epoll_ctl(m_epfd,op,fd,&epevent);
		if(rt){
			SYLAR_LOG_ERROR(g_logger)<<"epoll_ctl("<<m_epfd<<","<<op<<","<<fd<<","
				<<&epevent.events<<"): "<<rt<<" ("<<errno<<") ("<<strerror(errno)<<")";
			return false;	
		}

		if(fd_ctx->events & READ){
			fd_ctx->triggerEvent(READ);
			--m_pendingEventCount; 
		}

		if(fd_ctx->events & WRITE){
			fd_ctx->triggerEvent(WRITE);
			--m_pendingEventCount; 
		}
		SYLAR_ASSERT(fd_ctx->events == 0); 
		return true;
	}

	//获取当前的IOManager
	IOManager* IOManager::GetThis(){
		return dynamic_cast<IOManager*> (Scheduler::GetThis());
	}

	void IOManager::tickle() {
		if(hasIdleThreads()){
			return;
		}
		int rt = write(m_tickleFds[1],"T",1);
		SYLAR_ASSERT(rt == 1);	
	}
	
	bool IOManager::stopping(uint64_t& timeout){
		timeout = getNextTimer();
		return timeout == ~0ull
			&& m_pendingEventCount == 0
			&& Scheduler::stopping();
	}

	bool IOManager::stopping() {
		uint64_t timeout = 0;
		return stopping(timeout);		
	}

	//当协程无事可做时会陷入idle,此时需要处理epoll机制
	void IOManager::idle() {
		SYLAR_LOG_DEBUG(g_logger) << "idle";
		const uint64_t MAX_EVENTS = 256;
		epoll_event* events = new epoll_event[MAX_EVENTS]();
		//自定义删除器,不使用此智能指针，只是为了离开之后释放内存
		std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr){
				delete[] ptr;
				});


		while(true){
			uint64_t next_timeout = 0;
			if(stopping(next_timeout)){     //IOManager::stopping()
					SYLAR_LOG_INFO(g_logger)<<"name ="<<getName()<<" idle stopping exit";
					break;
			}


			int rt = 0;
			do{
				static const int MAX_TIMEOUT = 3000;
				//返回的实际长度
				if(next_timeout != ~0ull){
					next_timeout = (int)next_timeout>MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
				}else{
					next_timeout = MAX_TIMEOUT;
				}
				rt = epoll_wait(m_epfd,events,MAX_EVENTS,(int)next_timeout);

				if(rt<0 && errno == EINTR){
				}else{
					break;
				}
			}while(true);
			
			//next_timeout触发
			std::vector<std::function<void()>>cbs;
			listExpiredCb(cbs);
			if(!cbs.empty()){
				schedule(cbs.begin(),cbs.end());
				cbs.clear();
			}

			for(int i=0;i<rt;++i){
				epoll_event& event = events[i];
				//验证是否是唤醒
				if(event.data.fd == m_tickleFds[0]){
					uint8_t dummy;
					while(read(m_tickleFds[0],&dummy,1) == 1);			//"T"没有实际意义
					continue;
				}

				FdContext* fd_ctx = (FdContext*)event.data.ptr;						//取回原来的数据
				FdContext::MutexType::Lock lock(fd_ctx->mutex);
				if(event.events & (EPOLLERR|EPOLLHUP)){					//错误或中断
					event.events |=EPOLLIN|EPOLLOUT;					//唤醒读或写
				}					
				int real_events = NONE;
				if(event.events&EPOLLIN){
					real_events |= READ;
				}
				if(event.events&EPOLLOUT){
					real_events |= WRITE;
				}

				if((fd_ctx->events & real_events)==NONE){
					continue;
				}

				int left_events = (fd_ctx->events&~real_events);
				int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
				event.events = EPOLLET | left_events;

				int rt2 = epoll_ctl(m_epfd,op,fd_ctx->fd,&event);
				if(rt2){
					SYLAR_LOG_ERROR(g_logger)<<"epoll_ctl("<<m_epfd<<","
						<<op<<","<<fd_ctx->fd<<","
						<<event.events<<"): "
						<<rt2<<" ("<<errno<<") ("<<strerror(errno)<<")";
					continue;
				}
				//没有出错事件也已经修改，查看有哪些事件需要触发
				if(real_events & READ){
					fd_ctx->triggerEvent(READ);
					--m_pendingEventCount;
				}
				if(real_events & WRITE){
					fd_ctx->triggerEvent(WRITE);
					--m_pendingEventCount;
				}
			}
			Fiber::ptr cur = Fiber::GetThis();
			auto raw_ptr = cur.get();
			cur.reset();

			raw_ptr->swapOut();
			//从idle_fiber->swapIn()处返回	
		}
	}

	void IOManager::onTimerInsertedAtFront(){
		//唤醒epoll_wait重新设置定时时间
		tickle();
	}	

}
