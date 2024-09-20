#include "Common/Config.h"

#include "Common/SystemCallException.h"
#include "Common/Stream/NamedPipeStream.h"
//#include "Common/Thread/Cancellation.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


NamedPipeStream::NamedPipeStream(const char *name, bool serverSide)
:
  itsReadName(std::string(name) + (serverSide ? "-0" : "-1")),
  itsWriteName(std::string(name) + (serverSide ? "-1" : "-0")),
  itsReadStream(0),
  itsWriteStream(0)
{
  try {
    if (mknod(itsReadName.c_str(), 0600 | S_IFIFO, 0) < 0 && errno != EEXIST)
      throw SystemCallException(std::string("mknod ") + itsReadName, errno);

    if (mknod(itsWriteName.c_str(), 0600 | S_IFIFO, 0) < 0 && errno != EEXIST)
      throw SystemCallException(std::string("mknod ") + itsWriteName, errno);

    itsReadStream = new FileStream(itsReadName.c_str(), O_RDWR, 0600); // strange; O_RDONLY hangs ???
    itsWriteStream = new FileStream(itsWriteName.c_str(), O_RDWR, 0600);
  } catch (...) {
    cleanUp();
    throw;
  }
}


NamedPipeStream::~NamedPipeStream() noexcept(false)
{
  //ScopedDelayCancellation dc; // unlink is a cancellation point

  cleanUp();
}


void NamedPipeStream::cleanUp()
{
  unlink(itsReadName.c_str());
  unlink(itsWriteName.c_str());
  delete itsReadStream;
  delete itsWriteStream;
}


size_t NamedPipeStream::tryRead(void *ptr, size_t size)
{
  return itsReadStream->tryRead(ptr, size);
}


size_t NamedPipeStream::tryWrite(const void *ptr, size_t size)
{
  return itsWriteStream->tryWrite(ptr, size);
}


void NamedPipeStream::sync()
{
  itsWriteStream->sync();
}
