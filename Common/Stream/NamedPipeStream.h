#ifndef STREAM_NAMED_PIPE_STREAM_H
#define STREAM_NAMED_PIPE_STREAM_H

#include "Common/Stream/FileStream.h"

#include <string>


class NamedPipeStream : public Stream
{
  public:
		   NamedPipeStream(const char *name, bool serverSide);
    virtual	   ~NamedPipeStream() noexcept(false);

    virtual size_t tryRead(void *, size_t), tryWrite(const void *, size_t);

    virtual void   sync();

  private:
    void	   cleanUp();

    std::string    itsReadName, itsWriteName;
    FileStream	   *itsReadStream, *itsWriteStream;
};

#endif
