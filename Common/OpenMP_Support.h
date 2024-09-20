#ifndef OPEN_MP_SUPPORT_H
#define OPEN_MP_SUPPORT_H

#include <omp.h>


namespace omp
{
  class Mutex
  {
    public:
      Mutex()
      {
	omp_init_lock(&omp_lock);
      }

      ~Mutex()
      {
	omp_destroy_lock(&omp_lock);
      }

      void lock()
      {
	omp_set_lock(&omp_lock);
      }

      void unlock()
      {
	omp_unset_lock(&omp_lock);
      }

    private:
      omp_lock_t omp_lock;
  };


  class NestedMutex
  {
    public:
      NestedMutex()
      {
	omp_init_nest_lock(&omp_lock);
      }

      ~NestedMutex()
      {
	omp_destroy_nest_lock(&omp_lock);
      }

      void lock()
      {
	omp_set_nest_lock(&omp_lock);
      }

      void unlock()
      {
	omp_unset_nest_lock(&omp_lock);
      }

    private:
      omp_nest_lock_t omp_lock;
  };


  template <typename mutex_type = Mutex> class ScopedLock
  {
    public:
      ScopedLock(mutex_type &omp_lock)
      :
	omp_lock(omp_lock)
      {
	omp_lock.lock();
      }

      ~ScopedLock()
      {
	omp_lock.unlock();
      }

    private:
      mutex_type &omp_lock;
  };
}

#endif
