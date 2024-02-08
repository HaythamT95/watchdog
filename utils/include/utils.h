#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h> /*fputs*/

#ifndef NDEBUG
#define DEBUG_ONLY(x) x
#else
#define DEBUG_ONLY(x) do {} while(0)
#endif

#define BAD_MEM(mem) (mem)0xBADC0FFEE0DDF00D

#define RET_IF_BAD(IS_GOOD, RET_VAL) if (!(IS_GOOD)) { DEBUG_ONLY(fprintf(stderr,"RET_IF_BAD failed with: " #IS_GOOD) ;) return RET_VAL; }
#endif /*__UTILS_H__*/