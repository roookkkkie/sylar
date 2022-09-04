#ifndef __SYLAR_MACRO_H_
#define __SYLAR_MACRO_H_

#include<string.h>
#include<assert.h>
#include"util.h"

#if defined __GNUC__ || defined __llvm__
# define SYLAR_LIKELY(x)	__builtin_expect(!!(x),1)
# define SYLAR_UNLIKELY(x)  __builtin_expect(!!(x),0)
#else
# define SYLAR_LIKELY(x)	(x)
# define SYLAR_UNLIKELY(x)  (x)
#endif 

#define SYLAR_ASSERT(x) \
	if(SYLAR_UNLIKELY(!(x))){ \
		SYLAR_LOG_ERROR(SYLAR_LOG_ROOT())<<" ASERTION: " #x \
			<<"\nbacktrace:\n" <<sylar::BacktraceToString(100,2,"    "); \
		assert(x); \
	}

#define SYLAR_ASSERT2(x,w) \
	if(SYLAR_UNLIKELY(!(x))){ \
		SYLAR_LOG_ERROR(SYLAR_LOG_ROOT())<<" ASERTION: " #x \
			<<"\n"<<w <<"\nbacktrace:\n" <<sylar::BacktraceToString(100,2,"    "); \
		assert(x); \
	}

#endif
