#ifndef COMMON_ALIGNED_ALLOCATOR_H
#define COMMON_ALIGNED_ALLOCATOR_H

#include "Common/SystemCallException.h"

#include <cstdlib>
#include <memory>


template <typename T, size_t ALIGNMENT> class AlignedStdAllocator : public std::allocator<T>
{
  // NOTE: An std::allocator cannot hold *any* state because they're
  // constructed and destructed at will by the STL. The STL does not
  // even guarantee that the same allocator object will allocate and
  // deallocate a certain pointer.
  public:
    typedef typename std::allocator<T>::size_type size_type;
    typedef typename std::allocator<T>::pointer pointer;
    typedef typename std::allocator<T>::const_pointer const_pointer;

    template <class U> struct rebind
    {
      typedef AlignedStdAllocator<U, ALIGNMENT> other;
    };

    pointer allocate(size_type size, const_pointer /*hint*/ = 0)
    {
      void *ptr;
      int  retval;

      if ((retval = posix_memalign(&ptr, ALIGNMENT, size * sizeof(T))) != 0)
	throw SystemCallException("posix_memalign", retval);

      return static_cast<pointer>(ptr);
    }

    void deallocate(pointer ptr, size_type /*size*/)
    {
      free(ptr);
    }
};

#endif
