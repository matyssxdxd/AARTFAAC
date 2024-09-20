#include "Common/Config.h"

#include "Common/Stream/Stream.h"


Stream::EndOfStreamException::EndOfStreamException(const std::string &msg)
:
  Exception(msg)
{
}


Stream::EndOfStreamException::~EndOfStreamException() noexcept
{
}


Stream::~Stream() noexcept(false)
{
}


void Stream::read(void *ptr, size_t size)
{
  while (size > 0) {
    size_t bytes = tryRead(ptr, size);

    size -= bytes;
    ptr   = static_cast<char *>(ptr) + bytes;
  }
}


void Stream::write(const void *ptr, size_t size)
{
  while (size > 0) {
    size_t bytes = tryWrite(ptr, size);

    size -= bytes;
    ptr   = static_cast<const char *>(ptr) + bytes;
  }
}


std::string Stream::readLine()
{
  // TODO: do not do a system call per character

  std::string str;
  char	      ch;

  for (;;) {
    tryRead(&ch, 1);

    if (ch == '\n')
      return str;

    str += ch;
  }
}


void Stream::sync()
{
}
