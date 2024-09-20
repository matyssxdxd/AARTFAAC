#include <Common/Config.h>

#include <Common/LockedRanges.h>

#include <iostream>
#include <omp.h>


LockedRanges::LockedRanges(unsigned bufferSize)
:
  bufferSize(bufferSize)
{
}


void LockedRanges::lock(unsigned begin, unsigned end)
{
  std::unique_lock<std::mutex> lock(mutex);

  if (begin < end) {
    while (lockedRanges.subset(begin, end).count() > 0) {
#pragma omp critical (clog)
      std::clog << "Circular buffer: reader & writer try to use overlapping sections, range to lock = (" << begin << ", " << end << "), already locked = " << lockedRanges << std::endl;
      rangeUnlocked.wait(lock);
    }

    lockedRanges.include(begin, end);
  } else {
    while (lockedRanges.subset(begin, bufferSize).count() > 0 || lockedRanges.subset(0, end).count() > 0) {
#pragma omp critical (clog)
      std::clog << "Circular buffer: reader & writer try to use overlapping sections, range to lock = (" << begin << ", " << end << "), already locked = " << lockedRanges << std::endl;
      rangeUnlocked.wait(lock);
    }

    lockedRanges.include(begin, bufferSize).include(0, end);
  }
}


void LockedRanges::unlock(unsigned begin, unsigned end)
{
  std::unique_lock<std::mutex> lock(mutex);
  
  if (begin < end)
    lockedRanges.exclude(begin, end);
  else
    lockedRanges.exclude(end, bufferSize).exclude(0, begin);

  rangeUnlocked.notify_all();
}
