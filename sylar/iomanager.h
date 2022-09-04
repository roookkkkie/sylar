#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include"scheduler.h"
#include"timer.h"

namespace sylar{
	class IOManager: public Scheduler,public TimerManager{
		public:
			typedef std::shared_ptr<IOManager> ptr;
			typedef RWMutex RWMutexType;

			//自定义支持的事件
			enum Event{
				NONE = 0x0,
				READ = 0x1,								//EPOLLIN
				WRITE = 0x4								//EPOLLOUT
			};

		private:
			//事件的类来包装事件
			//fdContext是用来从epoll添加或者删除的
			struct FdContext{
				typedef Mutex MutexType;
				//事件的实例
				struct EventContext{
					//支持多协程调度器，需要知道自己的调度器
					Scheduler* scheduler = nullptr;		//事件执行的scheduler
					Fiber::ptr fiber;					//事件协程
					std::function<void()> cb;			//事件的回调函数
				};
				
				//拿到它是读事件还是写事件
				EventContext& getContext(Event event);
				void resetContext(EventContext& ctx); 
				void triggerEvent(Event event);

				int fd = 0;								//事件关联的句柄
				EventContext read;						//读事件
				EventContext write;						//写事件
				Event events = NONE;					//已注册的事件，表示在读还是在写
				MutexType mutex;
			};
		public:
			IOManager(size_t threads = 1,bool use_caller = true,const std::string name = "");
			~IOManager();

			//0 success  -1 error
			int addEvent(int fd,Event event,std::function<void()>cb = nullptr);
			//删除事件 
			bool delEvent(int fd,Event event);
			//取消事件,强制触发其操作
			bool cancelEvent(int fd,Event event);

			bool cancelAll(int fd);

			//获取当前的IOManager
			static IOManager* GetThis();

		protected:
			void tickle() override;
			bool stopping() override;
			void idle() override; 
			void onTimerInsertedAtFront() override;	
			void contextResize(size_t size);
			bool stopping(uint64_t& timeout);
		private:
			int m_epfd = 0;											//epoll fd
			int m_tickleFds[2];										//当陷于epoll_wait,有任务时来唤醒
			std::atomic<size_t> m_pendingEventCount = {0};			//当前等待执行的事件数量
			std::vector<FdContext*>	m_fdContexts;					//每个句柄一个上下文的数组
			RWMutexType m_mutex;
	};	

}
#endif
