#ifndef  COMMON_THREADS_SEMAPHORE_H
#define  COMMON_THREADS_SEMAPHORE_H

#include <condition_variable>
#include <mutex>

 
class Semaphore
{
  public:
    Semaphore(unsigned level = 0);
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator = (const Semaphore&) = delete;

    void up(unsigned count = 1);
    void down(unsigned count = 1);

  private:
    std::mutex 		    mutex;
    std::condition_variable condition;
    unsigned		    level;
};


inline Semaphore::Semaphore(unsigned level)
:
  level(level)
{
}


inline void Semaphore::up(unsigned count)
{
  std::lock_guard<std::mutex> lock(mutex);
  level += count;
  condition.notify_all();
}


inline void Semaphore::down(unsigned count)
{
  std::unique_lock<std::mutex> lock(mutex);
  condition.wait(lock, [this, count] { return level >= count; });
  level -= count;
}

#endif
