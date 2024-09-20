#if !defined SIGNAL_H
#define SIGNAL_H

#include <csignal>

#include "Common/SystemCallException.h"


class SignalHandler
{
  public:
    SignalHandler(int signum, void (*) (int));
    SignalHandler(const SignalHandler &) = delete;
    ~SignalHandler() noexcept(false);

  private:
    int              signum;
    struct sigaction oldact;
};


inline SignalHandler::SignalHandler(int signum, void (*handler) (int))
:
  signum(signum)
{
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = handler;

  if (sigaction(signum, &sa, &oldact) < 0)
    throw SystemCallException("sigaction");
}


inline SignalHandler::~SignalHandler() noexcept(false)
{
  if (sigaction(signum, &oldact, 0) < 0 && !std::uncaught_exception())
    throw SystemCallException("sigaction");
}

#endif
