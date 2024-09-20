#ifndef COMMON_THREADS_CONDITION_H
#define COMMON_THREADS_CONDITION_H

#include "Common/SystemCallException.h"
#include "Common/Threads/Mutex.h"

#include <pthread.h>


class Condition
{
  public:
    Condition(), ~Condition();

    void signal(), broadcast();
    void wait(Mutex &);
    bool wait(Mutex &, const struct timespec &);

  private:
    pthread_cond_t condition;
};


inline Condition::Condition()
{
  int error = pthread_cond_init(&condition, 0);

  if (error != 0)
    throw SystemCallException("pthread_cond_init", error);
}


inline Condition::~Condition()
{
  (void) pthread_cond_destroy(&condition);
}


inline void Condition::signal()
{
  int error = pthread_cond_signal(&condition);

  if (error != 0)
    throw SystemCallException("pthread_cond_signal", error);
}


inline void Condition::broadcast()
{
  int error = pthread_cond_broadcast(&condition);

  if (error != 0)
    throw SystemCallException("pthread_cond_broadcast", error);
}


inline void Condition::wait(Mutex &mutex)
{
  int error = pthread_cond_wait(&condition, &mutex.mutex);

  if (error != 0)
    throw SystemCallException("pthread_cond_wait", error);
}


inline bool Condition::wait(Mutex &mutex, const struct timespec &timespec)
{
  int error = pthread_cond_timedwait(&condition, &mutex.mutex, &timespec);

  switch (error) {
    case 0	   : return true;

    case ETIMEDOUT : return false;

    default	   : throw SystemCallException("pthread_cond_timedwait", error);
  }
}

#endif
