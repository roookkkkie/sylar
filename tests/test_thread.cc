#include "sylar/sylar.h"
#include <unistd.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int count = 0;
//sylar::RWMutex s_mutex;
sylar::Mutex s_mutex;

void fun1() {
    SYLAR_LOG_INFO(g_logger) << "name: " << sylar::Thread::GetName()
                             << " this.name: " << sylar::Thread::GetThis()->getName()
                             << " id: " << sylar::GetThreadId()
                             << " this.id: " << sylar::Thread::GetThis()->getId();

    for(int i = 0; i < 100000; ++i) {
        //sylar::RWMutex::WriteLock lock(s_mutex);
        sylar::Mutex::Lock lock(s_mutex);
        ++count;
    }
}

void fun2() {
    while(true) {
        SYLAR_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
        sleep(2);
    }
}

void fun3() {
    while(true) {
        SYLAR_LOG_INFO(g_logger) << "========================================";
        sleep(2);
    }
}

int main(int argc, char** argv) {
    SYLAR_LOG_INFO(g_logger) << "thread test begin";
  //  YAML::Node root = YAML::LoadFile("/home/lipei/myweb/bin/conf/log2.yml");
	// sylar::Config::LoadFromYaml(root);

   std::vector<sylar::Thread::ptr> thrs;
   //for(int i = 0; i < 2; ++i) {
   //    sylar::Thread::ptr thr(new sylar::Thread(&fun1, "name_" + std::to_string(i * 2)));
   //    sylar::Thread::ptr thr2(new sylar::Thread(&fun1, "name_" + std::to_string(i * 2 + 1)));
   //    thrs.push_back(thr);
   //    thrs.push_back(thr2);
   //}
    
    sylar::Thread::ptr thr(new sylar::Thread(&fun2, "name_1"));
     sylar::Thread::ptr thr2(new sylar::Thread(&fun3, "name_2"));
     thrs.push_back(thr);
     thrs.push_back(thr2);

    for(size_t i = 0; i < thrs.size(); ++i) {
        thrs[i]->join();
    }
    SYLAR_LOG_INFO(g_logger) << "thread test end";
	SYLAR_LOG_INFO(g_logger) << "count=" << count;
	sylar::Config::Visit([](sylar::ConfigVarBase::ptr var) {
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "name=" << var->getName()
                    << " description=" << var->getDescription()
                    << " typename=" << var->getTypeName()
                    << " value=" << var->toString();
    });

    return 0;
}
