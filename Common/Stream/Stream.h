#ifndef STREAM_STREAM_H
#define STREAM_STREAM_H

#include <string>

#include "Common/Exceptions/Exception.h"


class Stream
{
  public:
    class EndOfStreamException : public Exception
    {
      public:
	EndOfStreamException(const std::string &msg);
	~EndOfStreamException() noexcept;
    };

    virtual	   ~Stream() noexcept(false);

    virtual size_t tryRead(void *ptr, size_t size) = 0;
    void	   read(void *ptr, size_t size); // does not return until all bytes are read

    virtual size_t tryWrite(const void *ptr, size_t size) = 0;
    void	   write(const void *ptr, size_t size); // does not return until all bytes are written

    std::string    readLine(); // excludes '\n'

    virtual void   sync();
};

#endif
