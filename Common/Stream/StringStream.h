#ifndef STREAM_STRING_STREAM_H
#define STREAM_STRING_STREAM_H

#include "Common/Stream/Stream.h"

#include <sstream>

class StringStream : public Stream
{
  public:
    virtual ~StringStream() noexcept(false);

    virtual size_t tryRead(void *ptr, size_t size);
    virtual size_t tryWrite(const void *ptr, size_t size);

  private:
    std::stringstream itsBuffer;
};

#endif
