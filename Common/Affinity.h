#ifndef AFFINITY_H
#define AFFINITY_H

#if defined __linux__

#include <sched.h>

void setScheduler(int policy, int priority);
unsigned getNUMAnodeOfPCIeDevice(unsigned bus, unsigned device, unsigned function);
cpu_set_t getCPUlistOfPCIeDevice(unsigned bus, unsigned device, unsigned function);
unsigned currentCPU(), currentNode();
unsigned node(void *);


// bind to a CPU set as long as an object of this class is alive

class BoundThread
{
  public:
    BoundThread(const cpu_set_t &);
    ~BoundThread() noexcept(false);

  private:
    cpu_set_t	  old_cpuset;
#if 0
    int		  old_mode;
    unsigned long old_nodemask;
#endif
};

#endif

#endif
