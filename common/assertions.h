#ifndef __COMMON_UTILS_ASSERTIONS__H__
#define __COMMON_UTILS_ASSERTIONS__H__

#include <stdio.h>
#include <stdlib.h>

inline static void exit_function(const char *file, const char *func, const int line, const int exit_no) {

    fprintf(stderr, "%s:%d exit from function %s\n", file, line, func);
    exit(exit_no);
}

#define _Assert_Exit_							\
  if (getenv("GDBSTACKS")) {						\
    char tmp [1000];							\
    sprintf(tmp,"gdb -ex='set confirm off' -ex 'thread apply all bt' -ex q -p %d < /dev/null", getpid());  \
    __attribute__((unused)) int dummy=system(tmp);						\
  }									\
  fprintf(stderr, "\nExiting execution\n");				\
  fflush(stdout);							\
  fflush(stderr);							\
  exit_function(__FILE__, __FUNCTION__, __LINE__, 1); \
  abort(); // to avoid gcc warnings - never executed unless app-specific exit_function() does not exit() nor abort()

#define _Assert_(cOND, aCTION, fORMAT, aRGS...)             \
do {                                                        \
    if (!(cOND)) {                                          \
        fprintf(stderr, "\nAssertion (%s) failed!\n"   \
                "In %s() %s:%d\n" fORMAT,                   \
                #cOND, __FUNCTION__, __FILE__, __LINE__, ##aRGS);  \
        aCTION;                                             \
    }						\
} while(0)

#define AssertFatal(cOND, fORMAT, aRGS...) _Assert_(cOND, _Assert_Exit_, fORMAT, ##aRGS)

#endif 