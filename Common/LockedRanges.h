#ifndef COMMON_LOCKED_RANGES_H
#define COMMON_LOCKED_RANGES_H

#include <Common/SparseSet.h>

#include <condition_variable>
#include <mutex>


class LockedRanges
{
  public:
    LockedRanges(unsigned bufferSize);

    void lock(unsigned begin, unsigned end);
    void unlock(unsigned begin, unsigned end);

  private:
    SparseSet<unsigned>     lockedRanges;
    std::mutex		    mutex;
    std::condition_variable rangeUnlocked;
    const unsigned	    bufferSize;
};

#endif
