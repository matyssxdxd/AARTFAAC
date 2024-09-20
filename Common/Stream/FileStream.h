#ifndef STREAM_FILE_STREAM_H
#define STREAM_FILE_STREAM_H

#include "Common/Stream/FileDescriptorBasedStream.h"
#include <string>


class FileStream : public FileDescriptorBasedStream
{
  public:
	    FileStream(const std::string &name); // read-only; existing file
	    FileStream(const std::string &name, int mode); // rd/wr; create file
	    FileStream(const std::string &name, int flags, int mode); // rd/wr; create file, use given flags
						   
    virtual ~FileStream() noexcept(false);

    virtual void skip(size_t bytes);

    virtual size_t size();
};

#endif
