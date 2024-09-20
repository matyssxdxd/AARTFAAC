#include "Common/Config.h"

#include "Common/SystemCallException.h"
#include "Common/Stream/FileDescriptorBasedStream.h"
//#include <Common/Thread/Cancellation.h>

#include <exception>

#include <unistd.h>


FileDescriptorBasedStream::FileDescriptorBasedStream(int fd)
:
  fd(fd)
{
}


FileDescriptorBasedStream::~FileDescriptorBasedStream() noexcept(false)
{
  //ScopedDelayCancellation dc; // close() can throw as it is a cancellation point

  if (fd >= 0 && close(fd) < 0 && !std::uncaught_exception())
    throw SystemCallException("close", errno);
}


size_t FileDescriptorBasedStream::tryRead(void *ptr, size_t size)
{
  ssize_t bytes = ::read(fd, ptr, size);
  
  if (bytes < 0)
    throw SystemCallException("read", errno);

  if (bytes == 0) 
    throw EndOfStreamException("read");

  return bytes;
}


size_t FileDescriptorBasedStream::tryWrite(const void *ptr, size_t size)
{
  ssize_t bytes = ::write(fd, ptr, size);

  if (bytes < 0)
    throw SystemCallException("write", errno);

  return bytes;
}


void FileDescriptorBasedStream::sync()
{
  if (fsync(fd) < 0)
    throw SystemCallException("fsync", errno);
}
