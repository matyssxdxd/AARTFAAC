#ifndef STREAM_NULL_STREAM_H
#define STREAM_NULL_STREAM_H

#include "Common/Stream/Stream.h"


class NullStream : public Stream
{
  public:
    virtual	   ~NullStream() noexcept(false);

    virtual size_t tryRead(void *ptr, size_t size);
    virtual size_t tryWrite(const void *ptr, size_t size);
};

#endif
