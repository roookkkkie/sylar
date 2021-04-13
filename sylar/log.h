#ifndef _SYLAR_LOG_H_
#define _SYLAR_LOG_H_

#include<string>
#include<stdint.h>
#include<memory>
#include<list>
#include<sstream>
#include<fstream>
#include<vector>
#include<stdarg.h>
#include<map>
#include"util.h"
#include"singleton.h"
#include"thread.h"
/**
 * @brief 使用流式方式将日志级别level的日志写入到logger
 */
#define SYLAR_LOG_LEVEL(logger, level) \
	if(logger->getLevel() <= level) \
	sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
					__FILE__, __LINE__, 0, sylar::GetThreadId(),\
					sylar::GetFiberId(), time(0),sylar::Thread::GetName()))).getSS()

/**
 * @brief 使用流式方式将日志级别debug的日志写入到logger
 */
#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)

/**
 * @brief 使用流式方式将日志级别info的日志写入到logger
 */
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)

/**
 * @brief 使用流式方式将日志级别warn的日志写入到logger
 */
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)

/**
 * @brief 使用流式方式将日志级别error的日志写入到logger
 */
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)

/**
 * @brief 使用流式方式将日志级别fatal的日志写入到logger
 */
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

/**
 * @brief 使用格式化方式将日志级别level的日志写入到logger
 */
#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...) \
	if(logger->getLevel() <= level) \
	sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
					__FILE__, __LINE__, 0, sylar::GetThreadId(),\
					sylar::GetFiberId(), time(0),sylar::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别debug的日志写入到logger
 */
#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别info的日志写入到logger
 */
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别warn的日志写入到logger
 */
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别error的日志写入到logger
 */
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别fatal的日志写入到logger
 */
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()

#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)

namespace sylar{
	class Logger;
	class LoggerManager;
	class LogFormatter;
	//日志级别
	class LogLevel{
		public:
			enum Level{
				UNKNOW = 0,
				DEBUG = 1,
				INFO = 2,
				WARN = 3,
				ERROR = 4,
				FATAL = 5
			};
			static const char* ToString(LogLevel::Level level);
			static LogLevel::Level FromString(const std::string& str);
	};
	//日志事件
	class LogEvent{
		public:
			typedef std::shared_ptr<LogEvent> ptr;
			LogEvent(std::shared_ptr<Logger> logger,LogLevel::Level level,const char* file,int32_t line,uint32_t elapse,uint32_t threadId,uint32_t fiberId,uint64_t time,const std::string& thread_name);
			//~LogEvent();

			const char* getFile()const {return m_file;}
			int32_t getLine() const {return m_line;}
			uint32_t getElapse()const{return m_elapse;}
			uint32_t getThreadId()const{return m_threadId;}
			const std::string&  getThreadName()const{return m_threadName;}
			uint32_t getFiberId()const{return m_fiberId;}
			uint64_t getTime()const {return m_time;}
			std::string getContent() const{return m_ss.str();}
			std::stringstream& getSS() {return m_ss;}
			std::shared_ptr<Logger> getLogger()const{return m_logger;}
			LogLevel::Level getLevel()const {return m_level;}
			void format(const char* fmt,...);
			void format(const char* fmt,va_list al);
		private:
			const char* m_file = nullptr;	//文件名
			int32_t m_line = 0;				//行号
			uint32_t m_elapse = 0;			//程序启动开始到现在的毫秒数
			uint32_t m_threadId = 0;		//线程ID
			uint32_t m_fiberId = 0;			//协程ID
			uint64_t m_time;				//时间戳
			std::string m_threadName;
			std::stringstream  m_ss;	
			std::shared_ptr<Logger> m_logger;
			LogLevel::Level m_level;
	};
	//日志事件包装类型，利用析构函数触发日志写入
	class LogEventWrap{
		public:
			LogEventWrap(LogEvent::ptr e);
			~LogEventWrap();
			std::stringstream& getSS();
			LogEvent::ptr getEvent()const {return m_event;}
		private:
			LogEvent::ptr m_event;
	};
	//日志格式器	
	class LogFormatter{
		public:
			typedef std::shared_ptr<LogFormatter> ptr;
			LogFormatter(const std::string& pattern);
		    std::ostream& format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
			std::string format(std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event);
		public:
			class FormatItem{
				public:
					typedef std::shared_ptr<FormatItem> ptr;
					//FormatItem(const std::string& fmt=""){};
					virtual ~FormatItem(){}
					virtual void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) = 0;	
			};

			void init();

			bool isError()const {return m_error;}

			const std::string getPattern()const {return m_pattern;}
		private:
			std::string  m_pattern;
			std::vector<FormatItem::ptr> m_items;
			bool m_error = false;
	};
	//日志输出地
	class LogAppender{
		friend class Logger;
		public:
			typedef Spinlock MutexType;
			typedef std::shared_ptr<LogAppender> ptr;
			virtual ~LogAppender(){}
			virtual void log(std::shared_ptr<Logger>logger, LogLevel::Level level,LogEvent::ptr event) = 0;
			virtual std::string  toYamlString() = 0;

			void setFormatter(LogFormatter::ptr val);
			LogFormatter::ptr getFormatter();//const {return m_formatter;}
			LogLevel::Level getLevel()const {return m_level;}
			void setLevel(LogLevel::Level val){m_level = val;}
		protected:
			LogLevel::Level m_level = LogLevel::DEBUG;
			bool m_hasFormatter = false;
			MutexType m_mutex;
			LogFormatter::ptr m_formatter;
	};
	//日志器
	class Logger:public std::enable_shared_from_this<Logger>{
		friend class LoggerManager;
		public:
			typedef Spinlock MutexType;
			typedef std::shared_ptr<Logger> ptr;
			Logger(const std::string& name = "root");
			void log(LogLevel::Level level,LogEvent::ptr event);

			void debug(LogEvent::ptr event);
			void info(LogEvent::ptr event);	
			void warn(LogEvent::ptr event);	
			void error(LogEvent::ptr event);	
			void fatal(LogEvent::ptr event);	

			void addAppender(LogAppender::ptr appender);
			void delAppender(LogAppender::ptr appender);
			void clearAppenders();

			LogLevel::Level getLevel()const{ return m_level;}
			void setLevel(LogLevel::Level val){ m_level = val;}

			const std::string getName()const {return m_name;}

			void setFormatter(LogFormatter::ptr val);
			void setFormatter(const std::string& val);

			LogFormatter::ptr getFormatter();

			std::string toYamlString();

		private:
			std::string m_name;									//日志名称
			LogLevel::Level m_level;							//日志级别
			MutexType m_mutex;
			std::list<LogAppender::ptr> m_appenders;			//Appender集合
			LogFormatter::ptr m_formatter;
			Logger::ptr m_root;
	};
	//输出到控制台的appender
	class StdoutLogAppender:public LogAppender{
		public:
			typedef std::shared_ptr<StdoutLogAppender> ptr;
			void log(std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override;
			std::string toYamlString() override;
			
	};
	//定义输出到文件的appender
	class FileLogAppender:public LogAppender{
		public:
			typedef std::shared_ptr<FileLogAppender> ptr;
			FileLogAppender(const std::string& filename);
			void log(std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override;
			std::string toYamlString() override;

			bool reopen(); 
		private:
			std::string m_filename;
			std::ofstream m_filestream;
			uint64_t m_lastTime = 0;			//每隔一段时间，重新reopen/或者用信号
	};
	class LoggerManager{
		public:
			typedef Spinlock MutexType;
			LoggerManager();
			Logger::ptr getLogger(const std::string& name);

			void init();
			Logger::ptr getRoot()const{return m_root;}
			std::string toYamlString();
		private:
			MutexType m_mutex;
			std::map<std::string,Logger::ptr> m_loggers;
			Logger::ptr m_root;
	};

	typedef	sylar::Singleton<LoggerManager> LoggerMgr;
}

#endif
