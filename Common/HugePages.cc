#include "Common/Config.h"

#include "Common/Align.h"
#include "Common/HugePages.h"
#include "Common/SystemCallException.h"

#include <errno.h>
#include <asm/mman.h>
#include <sys/mman.h>


HugePages::HugePages(size_t size)
{
  allocated = align(size, 2048 * 1024);

  if ((ptr = mmap(nullptr, allocated, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON | MAP_HUGETLB, -1, 0)) == MAP_FAILED)
    throw SystemCallException("mmap");
}


HugePages::~HugePages() noexcept(false)
{
  if (munmap(ptr, allocated) != 0 && !std::uncaught_exception())
    throw SystemCallException("munmap");
}
