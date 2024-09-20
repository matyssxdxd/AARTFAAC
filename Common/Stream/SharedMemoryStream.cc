#include "Common/Config.h"

#include "Common/Stream/SharedMemoryStream.h"

#include <cstring>


SharedMemoryStream::~SharedMemoryStream() noexcept(false)
{
}


size_t SharedMemoryStream::tryRead(void *ptr, size_t size)
{
  std::lock_guard<std::mutex> lock(readLock);

  writePosted.down();
  readSize = std::min(size, writeSize);
  memcpy(ptr, writePointer, readSize);

  readDone.up();
  return readSize;
}


size_t SharedMemoryStream::tryWrite(const void *ptr, size_t size)
{
  std::lock_guard<std::mutex> lock(writeLock);

  writePointer = ptr;
  writeSize    = size;
  writePosted.up();

  readDone.down();
  return readSize;
}
