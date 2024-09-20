#ifndef COMMON_HUGE_PAGES_H
#define COMMON_HUGE_PAGES_H

#include <cstddef>


class HugePages
{
  public:
    HugePages(size_t size);
    ~HugePages() noexcept(false);

    operator void * () const
    {
      return ptr;
    }

  private:
    void   *ptr;
    size_t allocated;
};

#endif
