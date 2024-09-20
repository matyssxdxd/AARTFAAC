#ifndef ALIGN_H
#define ALIGN_H

#include <cstddef>

template <typename T> inline static bool powerOfTwo(T n)
{
  return (n | (n - 1)) == 2 * n - 1;
}


template <typename T> inline static T nextPowerOfTwo(T n)
{
  T p;

  for (p = 1; p < n; p <<= 1)
    ;

  return p;
}


template <typename T> inline static T align(T value, size_t alignment)
{
#if defined __GNUC__
  if (__builtin_constant_p(alignment) && powerOfTwo(alignment))
    return (value + alignment - 1) & ~(alignment - 1);
  else
#endif
    return (value + alignment - 1) / alignment * alignment;
}


template <typename T> inline static T *align(T *value, size_t alignment)
{
  return reinterpret_cast<T *>(align(reinterpret_cast<size_t>(value), alignment));
}


template <typename T> inline static bool aligned(T value, size_t alignment)
{
  return value % alignment == 0;
}


template <typename T> inline static bool aligned(T *value, size_t alignment)
{
  return reinterpret_cast<size_t>(value) % alignment == 0;
}

#endif
