#ifndef COMMON_SYSTEM_CALL_EXCEPTION_H
#define COMMON_SYSTEM_CALL_EXCEPTION_H

#include "Common/Exceptions/Exception.h"

#include <cerrno>
#include <string>


class SystemCallException : public Exception
{
  public:
			SystemCallException(const std::string &syscall,
					    int error = errno /*,
					    const std::string &file = "",
					    int line = 0,
					    const std::string &func = "",
					    Backtrace *bt = 0*/);
			
			SystemCallException(const SystemCallException &other);

    virtual		~SystemCallException();
    
    const int		error;

  private:
    static std::string	errorMessage(int error);
};

#if 0
#define SYSTEMCALLEXCEPTION_CLASS(excp,super)			\
  class excp : public super					\
  {								\
  public:							\
    excp(const std::string& syscall, int error = errno,         \
         const std::string& file="",                    	\
         int line=0, const std::string& function="",		\
         Backtrace* bt=0) :					\
      super(syscall, error, file, line, function, bt) {}        \
      virtual const std::string& type() const {			\
        static const std::string itsType(#excp);		\
        return itsType;						\
      }								\
  }
#endif

#endif
