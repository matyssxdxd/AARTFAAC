#ifndef COMMON_WALL_CLOCK_TIME_H
#define COMMON_WALL_CLOCK_TIME_H

#include "Common/TimeStamp.h"

#include <condition_variable>
#include <mutex>


class WallClockTime
{
  public:
	                    WallClockTime();
    bool                    waitUntil(const TimeStamp &);
    void                    cancelWait();

  private:
    std::mutex              mutex;
    std::condition_variable condition;
    bool	            cancelled;
};


inline WallClockTime::WallClockTime()
:
  cancelled(false)
{
}


inline bool WallClockTime::waitUntil(const TimeStamp &timestamp)
{
  std::unique_lock<std::mutex> lock(mutex);
  return !condition.wait_until(lock, TimeStamp::time_point(timestamp), [&] { return cancelled; });
}


inline void WallClockTime::cancelWait()
{
  std::lock_guard<std::mutex> lock(mutex);
  cancelled = true;
  condition.notify_all();
}

#endif
