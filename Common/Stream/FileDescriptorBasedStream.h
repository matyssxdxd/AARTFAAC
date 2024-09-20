#ifndef STREAM_FILE_DESRIPTOR_BASED_STREAM_H
#define STREAM_FILE_DESRIPTOR_BASED_STREAM_H

#include "Common/Stream/Stream.h"


class FileDescriptorBasedStream : public Stream
{
  public:
		   FileDescriptorBasedStream(int fd = -1);
    virtual	   ~FileDescriptorBasedStream() noexcept(false);

    virtual size_t tryRead(void *ptr, size_t size);
    virtual size_t tryWrite(const void *ptr, size_t size);

    virtual void   sync();

    int		   fd;
};

#endif
