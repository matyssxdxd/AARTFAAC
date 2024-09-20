#include "Common/Config.h"

#include "Common/Stream/StringStream.h"
//#include <Common/Thread/Cancellation.h"

#include <cstring>


StringStream::~StringStream() noexcept(false)
{
}


size_t StringStream::tryRead(void *ptr, size_t size)
{
  //Cancellation::point(); // keep behaviour consistent with real I/O streams

  return itsBuffer.readsome(static_cast<char*>(ptr), size);
}


size_t StringStream::tryWrite(const void *ptr, size_t size)
{
  //Cancellation::point(); // keep behaviour consistent with real I/O streams

  itsBuffer.write(static_cast<const char*>(ptr), size);

  return size;
}
