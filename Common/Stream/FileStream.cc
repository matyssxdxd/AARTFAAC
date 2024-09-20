#include "Common/Config.h"

#include "Common/SystemCallException.h"
#include "Common/Stream/FileStream.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


FileStream::FileStream(const std::string &name)
{
  if ((fd = open(name.c_str(), O_RDONLY)) < 0)
    throw SystemCallException(std::string("open ") + name, errno);
}


FileStream::FileStream(const std::string &name, int mode)
{
  if ((fd = open(name.c_str(), O_RDWR | O_CREAT | O_TRUNC, mode)) < 0)
    throw SystemCallException(std::string("open ") + name, errno);
}


FileStream::FileStream(const std::string &name, int flags, int mode)
{
  if ((fd = open(name.c_str(), flags, mode)) < 0) 
    throw SystemCallException(std::string("open ") + name, errno);
}


FileStream::~FileStream() noexcept(false)
{
}


void FileStream::skip(size_t bytes)
{
  // lseek returning -1 can be either an error, or theoretically,
  // a valid new file position. To make sure, we need to
  // clear and check errno.
  errno = 0;

  if (lseek(fd, bytes, SEEK_CUR) == (off_t) -1 && errno != 0)
    throw SystemCallException("lseek", errno);
}


size_t FileStream::size()
{
  struct stat stat;

  if (fstat(fd, &stat) != 0)
    throw SystemCallException("fstat", errno);

  return stat.st_size;
}
