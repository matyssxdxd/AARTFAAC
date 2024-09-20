#include "Common/Config.h"

#include "Common/SystemCallException.h"

#include <cstring>


SystemCallException::SystemCallException(const std::string &syscall, int error /*, const std::string& file, int line, 
			const std::string& func, Backtrace* bt*/)
: Exception(syscall + ": " + errorMessage(error) /*, file, line, func, bt*/),
  error(error)
{
}


SystemCallException::SystemCallException(const SystemCallException &other)
:
  Exception(other),
  error(other.error)
{
}


SystemCallException::~SystemCallException()
{
}


std::string SystemCallException::errorMessage(int error)
{
  char buffer[128];

  // there are two incompatible versions of versions of strerror_r()

#if !__linux__ || (!_GNU_SOURCE && (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600))
  if (strerror_r(error, buffer, sizeof buffer) == 0)
    return std::string(buffer);
  else
    return "could not convert error to string";
#else
  return strerror_r(error, buffer, sizeof buffer);
#endif
}
