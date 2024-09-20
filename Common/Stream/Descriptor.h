#ifndef COMMON_STREAM_DESCRIPTOR
#define COMMON_STREAM_DESCRIPTOR

#include "Common/Stream/Stream.h"
#include "Common/Exceptions/Exception.h"

#include <sys/time.h>

#include <string>


class BadDescriptor : public Exception
{
  public:
    BadDescriptor(const std::string &descriptor);
};


extern Stream *createStream(const std::string &descriptor, bool asReader, time_t deadline = 0);

#endif
