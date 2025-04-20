#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "common/assertions.h"
#include "common/log.h"
#include "common/thread_create.h"

int checkIfFedoraDistribution(void) {
    return !system("grep -iq 'ID_LIKE.*fedora' /etc/os-release ");
}
  
int checkIfGenericKernelOnFedora(void) {
    return system("uname -a | grep -q rt");
}
  
int checkIfInsideContainer(void) {
    return !system("egrep -q 'libpod|podman|kubepods'  /proc/self/cgroup");
}
  
void threadCreate(pthread_t* t, void * (*func)(void*), void * param, const char* name, int affinity, int priority){
  pthread_attr_t attr;
  int ret;
  int settingPriority = 1;
  ret=pthread_attr_init(&attr);
  AssertFatal(ret==0,"ret: %d, errno: %d\n",ret, errno);

  LOG_I("Creating thread %s with affinity %d and priority %d\n",name,affinity,priority);

  if (checkIfFedoraDistribution())
    if (checkIfGenericKernelOnFedora())
      if (checkIfInsideContainer())
        settingPriority = 0;
  
  if (settingPriority) {
    ret=pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    AssertFatal(ret==0,"ret: %d, errno: %d\n",ret, errno);
    ret=pthread_attr_setschedpolicy(&attr, SCHED_DEFAULT);
    AssertFatal(ret==0,"ret: %d, errno: %d\n",ret, errno);
    if(priority<sched_get_priority_min(SCHED_DEFAULT) || priority>sched_get_priority_max(SCHED_DEFAULT)) {
      LOG_E("Prio not possible: %d, min is %d, max: %d, forced in the range\n",
	    priority,
	    sched_get_priority_min(SCHED_DEFAULT),
	    sched_get_priority_max(SCHED_DEFAULT));
      if(priority<sched_get_priority_min(SCHED_DEFAULT))
        priority=sched_get_priority_min(SCHED_DEFAULT);
      if(priority>sched_get_priority_max(SCHED_DEFAULT))
        priority=sched_get_priority_max(SCHED_DEFAULT);
    }
    AssertFatal(priority<=sched_get_priority_max(SCHED_DEFAULT),"");
    struct sched_param sparam={0};
    sparam.sched_priority = priority;
    ret=pthread_attr_setschedparam(&attr, &sparam);
    AssertFatal(ret==0,"ret: %d, errno: %d\n",ret, errno);
  }
  
  ret=pthread_create(t, &attr, func, param);
  AssertFatal(ret==0,"ret: %d, errno: %d\n",ret, errno);
  
  pthread_setname_np(*t, name);
  if (affinity != -1 ) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(affinity, &cpuset);
    AssertFatal( pthread_setaffinity_np(*t, sizeof(cpu_set_t), &cpuset) == 0, "Error setting processor affinity");
  }
  pthread_attr_destroy(&attr);
}

// Block CPU C-states deep sleep
void set_latency_target(void) {
  int ret;
  static int latency_target_fd=-1;
  uint32_t latency_target_value=2; // in microseconds
  if (latency_target_fd == -1) {
    if ( (latency_target_fd = open("/dev/cpu_dma_latency", O_RDWR)) != -1 ) {
      ret = write(latency_target_fd, &latency_target_value, sizeof(latency_target_value));
      if (ret == 0) {
	printf("# error setting cpu_dma_latency to %u!: %s\n", latency_target_value, strerror(errno));
	close(latency_target_fd);
	latency_target_fd=-1;
	return;
      }
    }
  }
  if (latency_target_fd != -1) 
    LOG_I("# /dev/cpu_dma_latency set to %u us\n", latency_target_value);
  else
    LOG_E("Can't set /dev/cpu_dma_latency to %u us\n", latency_target_value);

  // Set CPU frequency to it's maximum
  int system_ret = system("for d in /sys/devices/system/cpu/cpu[0-9]*; do cat $d/cpufreq/cpuinfo_max_freq > $d/cpufreq/scaling_min_freq; done");
  if (system_ret == -1) {
    LOG_E("Can't set cpu frequency: [%d]  %s\n", errno, strerror(errno));
    return;
  }
  if (!((WIFEXITED(system_ret)) && (WEXITSTATUS(system_ret) == 0))) {
    LOG_E("Can't set cpu frequency\n");
  }
  mlockall(MCL_CURRENT | MCL_FUTURE);

}



/// @brief  Set priority of current thread
/// @param priority (int) from 1 (lowest) to 99 (highest Real Time)
void set_priority(int priority)
{
    struct sched_param param = {
        .sched_priority = priority,
    };

    if (sched_setscheduler(0, SCHED_RR, &param) == -1) {
        fprintf(stderr, "sched_setscheduler: %s\n", strerror(errno));
        abort();
    }
}