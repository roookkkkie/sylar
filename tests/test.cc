#include <iostream>
#include "../sylar/log.h"
#include "../sylar/util.h"

void test(){
	SYLAR_LOG_FATAL(SYLAR_LOG_ROOT())<<"test::before";

}

int main(int argc, char** argv) {
    sylar::Logger::ptr logger(new sylar::Logger);
    logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));
	//sylar::LogEvent::ptr event(new sylar::LogEvent(__FILE__,__LINE__,0,sylar::GetThreadId(),sylar::GetFiberId(),time(0)));
    //logger->log(sylar::LogLevel::DEBUG,event);
	
	sylar::FileLogAppender::ptr file_appender(new sylar::FileLogAppender("./log.txt"));
    //sylar::LogFormatter::ptr fmt(new sylar::LogFormatter("%d%T%p%T%m%n"));
    //file_appender->setFormatter(fmt);
    file_appender->setLevel(sylar::LogLevel::ERROR);

    logger->addAppender(file_appender);

    //sylar::LogEvent::ptr event(new sylar::LogEvent(__FILE__, __LINE__, 0, sylar::GetThreadId(), sylar::GetFiberId(), time(0)));
    //event->getSS() << "hello sylar log";
    //logger->log(sylar::LogLevel::DEBUG, event);
    //std::cout << "hello sylar log" << std::endl;
	//SYLAR_LOG_INFO(SYLAR_LOG_ROOT())<<"main::before";
	//SYLAR_LOG_FATAL(SYLAR_LOG_ROOT())<<"main::before";
	SYLAR_LOG_INFO(logger) << "test macro";
	SYLAR_LOG_ERROR(logger) << "test macro error";

    SYLAR_LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");
    auto l = sylar::LoggerMgr::GetInstance()->getLogger("xx");
    SYLAR_LOG_INFO(l) << "xxx";
	test();
    return 0;
}
