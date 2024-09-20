#ifndef COMMON_THREADS_MUTEX_H
#define COMMON_THREADS_MUTEX_H

#include "Common/SystemCallException.h"

#include <exception>
#include <pthread.h>


class Mutex
{
  public:
    enum Type {
      NORMAL = PTHREAD_MUTEX_NORMAL,
      RECURSIVE = PTHREAD_MUTEX_RECURSIVE,
      ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK,
      DEFAULT = PTHREAD_MUTEX_DEFAULT
    };
    
    Mutex(Type type = DEFAULT);
    ~Mutex();

    void lock();
    void unlock();
    bool trylock();

  private:
    friend class Condition;

    Mutex(const Mutex &);
    Mutex & operator = (const Mutex &);
    
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexattr;
};


class ScopedLock
{
  public:
    ScopedLock(Mutex &);
    ~ScopedLock();

  private:
    ScopedLock(const ScopedLock &);
    ScopedLock & operator = (const ScopedLock&);

    Mutex &mutex;
};


inline Mutex::Mutex(Mutex::Type type)
{
  int error = pthread_mutexattr_init(&mutexattr);

  if (error != 0)
    throw SystemCallException("pthread_mutexattr_init", error);
    
  error = pthread_mutexattr_settype(&mutexattr, type);

  if (error != 0)
    throw SystemCallException("pthread_mutexattr_settype", error);

  error = pthread_mutex_init(&mutex, &mutexattr);

  if (error != 0)
    throw SystemCallException("pthread_mutex_init", error);
}


inline Mutex::~Mutex()
{
  (void) pthread_mutex_destroy(&mutex);
  (void) pthread_mutexattr_destroy(&mutexattr);
}


inline void Mutex::lock()
{
  int error = pthread_mutex_lock(&mutex);

  if (error != 0)
    throw SystemCallException("pthread_mutex_lock", error);
}


inline void Mutex::unlock()
{
  int error = pthread_mutex_unlock(&mutex);

  if (error != 0)
    throw SystemCallException("pthread_mutex_unlock", error);
}


inline bool Mutex::trylock()
{
  int error = pthread_mutex_trylock(&mutex);

  switch (error) {
    case 0       : return true;

    case EBUSY   : return false;

    // According to the POSIX standard only pthread_mutex_lock() can return
    // EDEADLK. However, some Linux implementations also return EDEADLK when
    // calling pthread_mutex_trylock() on a locked error-checking mutex.
    case EDEADLK : return false;

    default      : throw SystemCallException("pthread_mutex_trylock", error);
  }
}


inline ScopedLock::ScopedLock(Mutex &mutex)
:
  mutex(mutex)
{
  mutex.lock();
}


inline ScopedLock::~ScopedLock()
{
  try {
    mutex.unlock();
  } catch (std::exception &) {
  }
}

#endif
