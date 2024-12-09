#include "Common/Config.h"

#if defined __linux__
#include "Common/Affinity.h"
#include "Common/SystemCallException.h"

#include <numa.h>
#include <numaif.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>


unsigned getNUMAnodeOfPCIeDevice(unsigned bus, unsigned device, unsigned function)
{
  char path[64];
  unsigned node;
  sprintf(path, "/sys/bus/pci/devices/0000:%02x:%02x.%01x/numa_node", bus, device, function);
  std::ifstream(path) >> node;
  return node;
}

#if 1
cpu_set_t getCPUlistOfPCIeDevice(unsigned bus, unsigned device, unsigned function)
{
  char path[64], line[1024], *ptr = line;

  sprintf(path, "/sys/bus/pci/devices/0000:%02x:%02x.%01x/local_cpulist", bus, device, function);
  std::ifstream(path).getline(line, sizeof line);

  cpu_set_t set;
  CPU_ZERO(&set);

  do {
    unsigned begin, end;

    switch (sscanf(ptr, "%u-%u", &begin, &end)) {
      case 0 :
#pragma omp critical (cerr)
      		std::cerr << "cannot parse CPU list" << std::endl;
		exit(1);

      case 1 :  CPU_SET(begin, &set);
		break;

      case 2 :  for (unsigned cpu = begin; cpu <= end; cpu ++)
		  CPU_SET(cpu, &set);

		break;
    }
  } while ((ptr = strchr(ptr, ',') + 1) != (char *) 1);

  return set;
}
#endif


void setScheduler(int policy, int priority)
{
  struct sched_param sched_param;
  sched_param.sched_priority = priority;

  if (sched_setscheduler(0, policy, &sched_param) < 0)
    throw SystemCallException("sched_setscheduler");
}


unsigned currentCPU()
{
  int cpu;

  if ((cpu = sched_getcpu()) < 0)
    throw SystemCallException("sched_getcpu");

  return cpu;
}


unsigned currentNode()
{
  return numa_node_of_cpu(currentCPU());
}


unsigned node(void *address)
{
  int node;
   
  if (get_mempolicy(&node, nullptr, 0, address, MPOL_F_NODE | MPOL_F_ADDR) < 0)
    if (errno == ENOSYS)
      node = 0;
    else
      throw SystemCallException("get_mempolicy");

  return node;
}


BoundThread::BoundThread(const cpu_set_t &cpuset)
{
  // CPU policy

  //unsigned oldCPU = currentCPU(), oldNode = currentNode(), oldMem = node(&oldCPU);

  if (sched_getaffinity(0, sizeof old_cpuset, &old_cpuset) < 0)
    throw SystemCallException("sched_getaffinity");

  if (sched_setaffinity(0, sizeof cpuset, &cpuset) < 0)
    throw SystemCallException("sched_setaffinity");

#if 0 // moving the stack to the new NUMA domain makes the system unstable

  unsigned long newNodes = 1 << currentNode(), oldNodes = ~newNodes;

  for (ptrdiff_t addr = (ptrdiff_t) &oldCPU & ~(getpagesize() - 1);; addr -= getpagesize()) {
    if (mbind((void *) addr, getpagesize(), MPOL_PREFERRED, &newNodes, 2, MPOL_MF_MOVE | MPOL_MF_STRICT) < 0)
      if (errno == EFAULT)
	break;
      else
	throw SystemCallException("mbind");
  }

  unsigned newCPU = currentCPU(), newNode = currentNode(), newMem = node(&oldCPU);
#pragma omp critical (clog)
  std::clog << "thread moved from " << oldCPU << '/' << oldNode << '@' << oldMem << " to " << newCPU << '/' << newNode << '@' << newMem << std::endl;
#endif

#if 0
  // memory policy

  // FIXME: I really do not understand how the nodemask argument of get_mempolicy works
  // man pages are not up to date

  if (get_mempolicy(&old_mode, &old_nodemask, sizeof old_nodemask, 0, 0) < 0)
    throw SystemCallException("get_mempolicy");

  unsigned node;

  if (getcpu(0, &node, 0) < 0)
    throw SystemCallException("getcpu");

  unsigned long nodemask = 0x1 << node;

  if (set_mempolicy(MPOL_BIND, &nodemask, sizeof nodemask) < 0)
    throw SystemCallException("set_mempolicy");
#endif
}


BoundThread::~BoundThread() noexcept(false)
{
  if (sched_setaffinity(0, sizeof old_cpuset, &old_cpuset) < 0 && !std::uncaught_exceptions())
    throw SystemCallException("sched_setaffinity");
    
#if 0
  if (set_mempolicy(old_mode, &old_nodemask, sizeof old_nodemask) < 0 && !std::uncaught_exceptions())
    throw SystemCallException("set_mempolicy");
#endif
}

#endif
