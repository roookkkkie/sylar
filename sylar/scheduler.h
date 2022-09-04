#ifndef __SYLAR_SCHEDULER_H__
#define __SYLAR_SCHEDULER_H__

#include"thread.h"
#include"fiber.h"
#include"macro.h"
#include<memory.h>
#include<vector>
#include<list>

namespace sylar{
	class Scheduler{
		public:
			typedef std::shared_ptr<Scheduler> ptr;
			typedef Mutex MutexType;
			Scheduler(size_t threads = 1,bool use_caller = true,const std::string name = "");
			//基类
			virtual ~Scheduler();

			const std::string& getName()const {return m_name;}

			static Scheduler* GetThis();
			static Fiber* GetMainFiber();

			void start();
			void stop();

			template<class FiberOrCb>
				void schedule(FiberOrCb fc,int thread = -1){
					bool need_tickle = false;
					{
						MutexType::Lock lock(m_mutex);
						need_tickle = scheduleNoLock(fc,thread);	
					}
					if(need_tickle){
						tickle();
					}
				}

			template<class InputIterator>
				void schedule(InputIterator begin,InputIterator end){
					bool need_tickle = false;
					{
						//上锁是为了确保连续的请求一定在一个连续的队列里按顺序执行
						MutexType::Lock lock(m_mutex);
						while(begin!=end){
							//取的是指针的地址，会执行swap操作
							//此处取||是需要将第一次放tickle的值保存到放完
							need_tickle = scheduleNoLock(&*begin,-1)||need_tickle;
							++begin;
						}
						if(need_tickle){
							tickle();
						}
					}
				}

			void switchTo(int thread = -1);
			std::ostream& dump(std::ostream& os);

		protected:
			virtual void tickle();
			void run();
			virtual bool stopping();
			void setThis();
			virtual void idle();
			bool hasIdleThreads() {return m_idleThreadCount >0;}
		private:
			template<class FiberOrCb>
				bool scheduleNoLock(FiberOrCb fc,int thread){
					bool need_tickle = m_fibers.empty();
					FiberAndThread ft(fc,thread);
					if(ft.fiber||ft.cb){
						m_fibers.push_back(ft);
					}
					return need_tickle;
				}

		private:
			struct FiberAndThread{
				Fiber::ptr fiber;
				std::function<void()> cb;
				int thread;

				FiberAndThread(Fiber::ptr f,int thr):
					fiber(f),
					thread(thr){}
				//传智能指针的智能指针是为了用swap操作,减少智能指针的引用
				FiberAndThread(Fiber::ptr* f,int thr):
					thread(thr){
						fiber.swap(*f);
					}

				FiberAndThread(std::function<void()> f,int thr):
					cb(f),
					thread(thr){}
				FiberAndThread(std::function<void()>* f,int thr):
					thread(thr){
						cb.swap(*f);
					}

				//默认构造函数，放在stl时，分配对象需要默认构造函数，否则无法初始化
				FiberAndThread():thread(-1){}

				void reset(){
					fiber = nullptr;
					cb = nullptr;
					thread = -1;
				}
			};

		private:
			MutexType m_mutex;
			std::vector<Thread::ptr> m_threads;
			std::list<FiberAndThread> m_fibers;
			//std::map<int,std::list<FiberAndThread> > m_thrFibers;
			Fiber::ptr m_rootFiber;
			std::string m_name;
		protected:
			std::vector<int> m_threadIds;
			size_t m_threadCount = 0;
			std::atomic<size_t>  m_activeThreadCount = {0};
			std::atomic<size_t>  m_idleThreadCount = {0};//空闲
			bool m_stopping = true;
			bool m_autoStop = false;
			int m_rootThread = 0;

	};
	class SchedulerSwitcher : public Noncopyable {
		public:
			SchedulerSwitcher(Scheduler* target = nullptr);
			~SchedulerSwitcher();
		private:
			Scheduler* m_caller;
	};


}

#endif

