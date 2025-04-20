#ifndef _THREAD_CREATE_H_
#define _THREAD_CREATE_H_

#include <pthread.h>

#define SCHED_DEFAULT SCHED_RR
#define PRIORITY_RT_LOW sched_get_priority_min(SCHED_DEFAULT)
#define PRIORITY_RT ((sched_get_priority_min(SCHED_DEFAULT)+sched_get_priority_max(SCHED_DEFAULT))/2)
#define PRIORITY_RT_MAX sched_get_priority_max(SCHED_DEFAULT)-2

void threadCreate(pthread_t* t, void * (*func)(void*), void * param, const char* name, int affinity, int priority);
void set_latency_target(void);

/// @brief  Set priority of current thread
/// @param priority (int) from 1 (lowest) to 99 (highest Real Time)
void set_priority(int priority);

#endif // _THREAD_CREATE_H_