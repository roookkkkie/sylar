#ifndef __SYLAR_THREAD_H__
#define __SYLAR_THREAD_H__
#include<thread>
#include<functional>
#include<memory>
#include<pthread.h>
#include<semaphore.h>
#include<atomic>
#include"fiber.h"
#include<list>
#include"noncopyable.h"

namespace sylar{

	//信号量
	class Semaphore: Noncopyable{
		public:
			Semaphore(uint32_t count = 0);
			~Semaphore();

			void wait();
			void notify();
		private:
			sem_t m_semaphore;

	};

	//构造时加锁，析构时解锁
	template<class T>
		struct ScopedLockImpl{
			public:
				ScopedLockImpl(T& mutex):
					m_mutex(mutex){
						m_mutex.lock();
						m_locked = true;
					}

				~ScopedLockImpl(){
					unlock();
				}

				void lock(){
					if(!m_locked){
						m_mutex.lock();
						m_locked = true;
					}
				}

				void unlock(){
					if(m_locked){
						m_mutex.unlock();
						m_locked = false;
					}
				}

			private:
				T& m_mutex;
				bool m_locked;
		};

	template<class T>
		struct ReadScopedLockImpl{
			public:
				ReadScopedLockImpl(T& mutex):
					m_mutex(mutex){
						m_mutex.rdlock();
						m_locked = true;
					}

				~ReadScopedLockImpl(){
					unlock();
				}

				void lock(){
					if(!m_locked){
						m_mutex.rdlock();
						m_locked = true;
					}
				}

				void unlock(){
					if(m_locked){
						m_mutex.unlock();
						m_locked = false;
					}
				}

			private:
				T& m_mutex;
				bool m_locked;
		};

	template<class T>
		struct WriteScopedLockImpl{
			public:
				WriteScopedLockImpl(T& mutex):
					m_mutex(mutex){
						m_mutex.wrlock();
						m_locked = true;
					}

				~WriteScopedLockImpl(){
					unlock();
				}

				void lock(){
					if(!m_locked){
						m_mutex.wrlock();
						m_locked = true;
					}
				}

				void unlock(){
					if(m_locked){
						m_mutex.unlock();
						m_locked = false;
					}
				}

			private:
				T& m_mutex;
				bool m_locked;
		};

	//互斥量
	class Mutex: Noncopyable{
		public:
			typedef ScopedLockImpl<Mutex> Lock;
			Mutex(){
				pthread_mutex_init(&m_mutex,nullptr);
			}

			~Mutex(){
				pthread_mutex_destroy(&m_mutex);
			}

			void lock(){
				pthread_mutex_lock(&m_mutex);
			}

			void unlock(){	
				pthread_mutex_unlock(&m_mutex);
			}

		private:
			pthread_mutex_t m_mutex;

	};

	class NullMutex: Noncopyable{
		public:
			typedef ScopedLockImpl<NullMutex> Lock;
			NullMutex(){}
			~NullMutex(){}
			void lock(){}
			void unlock(){}
	};

	//读写锁
	class RWMutex: Noncopyable{
		public:
			typedef ReadScopedLockImpl<RWMutex> ReadLock;
			typedef WriteScopedLockImpl<RWMutex> WriteLock;
			RWMutex(){
				pthread_rwlock_init(&m_lock,nullptr);
			}
			~RWMutex(){
				pthread_rwlock_destroy(&m_lock);
			}
			void rdlock(){
				pthread_rwlock_rdlock(&m_lock);
			}
			void wrlock(){
				pthread_rwlock_wrlock(&m_lock);
			}
			void unlock(){
				pthread_rwlock_unlock(&m_lock);
			}
		private:
			pthread_rwlock_t m_lock;
	};

	class NullRWMutex: Noncopyable{
		public:
			typedef ReadScopedLockImpl<NullRWMutex> ReadLock;
			typedef WriteScopedLockImpl<NullRWMutex> WriteLock;
			NullRWMutex(){};
			~NullRWMutex(){};
			void rdlock(){};
			void wrlock(){};
			void unlock(){};
	};

	//自旋锁
	class Spinlock: Noncopyable{
		public:
			typedef ScopedLockImpl<Spinlock> Lock;
			Spinlock(){
				pthread_spin_init(&m_mutex,0);
			}
			~Spinlock(){
				pthread_spin_destroy(&m_mutex);
			}
			void lock(){
				pthread_spin_lock(&m_mutex);
			}
			void unlock(){
				pthread_spin_unlock(&m_mutex);
			}
		private:
			pthread_spinlock_t m_mutex;

	};

	//原子锁
	class CASLock: Noncopyable{
		public:
			typedef ScopedLockImpl<CASLock> Lock;
			CASLock(){
				m_mutex.clear();	
			}
			~CASLock(){
			}
			void lock(){
				while(std::atomic_flag_test_and_set_explicit(&m_mutex,std::memory_order_acquire)){

				};
			}
			void unlock(){
				std::atomic_flag_clear_explicit(&m_mutex,std::memory_order_release);
			}
		private:
			volatile std::atomic_flag m_mutex;
	};
/*
	class Scheduler;
	class FiberSemaphore : Noncopyable {
		public:
			typedef Spinlock MutexType;

			FiberSemaphore(size_t initial_concurrency = 0);
			~FiberSemaphore();

			bool tryWait();
			void wait();
			void notify();

			size_t getConcurrency() const { return m_concurrency;}
			void reset() { m_concurrency = 0;}
		private:
			MutexType m_mutex;
			std::list<std::pair<Scheduler*, Fiber::ptr> > m_waiters;
			size_t m_concurrency;
	};
*/
	class Thread: Noncopyable{
		public:
			typedef std::shared_ptr<Thread> ptr;
			Thread(std::function<void()> cb,const std::string& name);
			~Thread();

			pid_t getId()const{ return m_id;}
			const std::string& getName() const{ return m_name;}

			void join();
			static Thread* GetThis();
			static const std::string& GetName();
			//静态的写方法，主线程不是自己创建的，如果name必须创建时拿到
			//就没办法给主线程命名，取变量为线程局部变量，不用类生成，局部变量也存在
			static void SetName(const std::string& name);
		private:
			//静态的执行方法
			static void* run(void* arg);
		private:
			//linux 在top命令里的线程号 
			pid_t m_id = -1;
			//pthread线程Id
			pthread_t m_thread = 0;
			std::function<void()> m_cb;
			std::string m_name;

			Semaphore m_semaphore;
	};

}

#endif
