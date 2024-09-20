#ifndef STREAM_SHARED_MEMORY_STREAM_H
#define STREAM_SHARED_MEMORY_STREAM_H

#include "Common/Stream/Stream.h"
#include "Common/Threads/Semaphore.h"

#include <mutex>


class SharedMemoryStream : public Stream
{
  public:
    virtual ~SharedMemoryStream() noexcept(false);

    virtual size_t tryRead(void *ptr, size_t size);
    virtual size_t tryWrite(const void *ptr, size_t size);

  private:
    std::mutex readLock, writeLock;
    Semaphore  readDone, writePosted;
    const void *writePointer;
    size_t     readSize, writeSize;
};

#endif
