#include "Common/Config.h"

#include "Common/Stream/NullStream.h"
//#include "Common/Thread/Cancellation.h"

#include <cstring>


NullStream::~NullStream() noexcept(false)
{
}


size_t NullStream::tryRead(void *ptr, size_t size)
{
  //Cancellation::point(); // keep behaviour consistent with non-null streams

  memset(ptr, 0, size);
  return size;
}


size_t NullStream::tryWrite(const void *, size_t size)
{
  //Cancellation::point(); // keep behaviour consistent with non-null streams

  return size;
}
