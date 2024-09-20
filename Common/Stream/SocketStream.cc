#include "Common/Config.h"

//#include <Common/Thread/Cancellation.h>
#include "Common/Stream/SocketStream.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>

#include <dirent.h>
#include <errno.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <cstdlib>

#include <boost/lexical_cast.hpp>

//# AI_NUMERICSERV is not defined on OS-X
#ifndef AI_NUMERICSERV
# define AI_NUMERICSERV 0
#endif


// port range for unused ports search
const int MINPORT = 10000;
const int MAXPORT = 30000;


static struct RandomState {
  RandomState() { 
    xsubi[0] = getpid();
    xsubi[1] = time(0);
    xsubi[2] = time(0) >> 16;
  }

  unsigned short xsubi[3];
} randomState;


SocketStream::TimeOutException::TimeOutException(const std::string &msg)
:
  Exception(msg)
{
}


SocketStream::SocketStream(const std::string &hostname, uint16_t _port, Protocol protocol, Mode mode, time_t deadline, const std::string &nfskey, bool doAccept)
:
  protocol(protocol),
  mode(mode),
  hostname(hostname),
  port(_port),
  nfskey(nfskey),
  listen_sk(-1)
{  
  struct addrinfo hints;
  bool            autoPort = (port == 0);

  // use getaddrinfo, because gethostbyname is not thread safe
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET; // IPv4
  hints.ai_flags = AI_NUMERICSERV; // we only use numeric port numbers, not strings like "smtp"

  if (protocol == TCP) {
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
  } else {
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
  }

  for(;;) {
    try {
      char portStr[16];
      int  retval;
      struct addrinfo *result;

      if (mode == Client && nfskey != "")
	port = boost::lexical_cast<uint16_t>(readkey(nfskey, deadline));

      if (mode == Server && autoPort)
	port = MINPORT + static_cast<unsigned short>((MAXPORT - MINPORT) * erand48(randomState.xsubi)); // erand48() not thread safe, but not a problem.

      snprintf(portStr, sizeof portStr, "%hu", port);

      if ((retval = getaddrinfo(hostname.c_str(), portStr, &hints, &result)) != 0) {
	std::stringstream str;
	str << "getaddrinfo " << hostname << ':' << port << ": " << gai_strerror(retval);
	throw SystemCallException(str.str(), retval == EAI_SYSTEM ? errno : retval);
      }

      // make sure result will be freed
      struct D {
	~D() {
	  freeaddrinfo(result);
	}

	struct addrinfo *result;
      } onDestruct = { result };
      (void) onDestruct;

      // result is a linked list of resolved addresses, we only use the first

      if ((fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) < 0)
	throw SystemCallException("socket", errno);

      if (mode == Client) {
	while (connect(fd, result->ai_addr, result->ai_addrlen) < 0)
	  if (errno == ECONNREFUSED) {
	    if (deadline > 0 && time(0) >= deadline)
	      throw TimeOutException("client socket");

	    if (usleep(999999) < 0) {
	      // interrupted by a signal handler -- abort to allow this thread to
	      // be forced to continue after receiving a SIGINT, as with any other
	      // system call in this constructor 
	      throw SystemCallException("sleep", errno);
	    }
	  } else
	    throw SystemCallException("connect", errno);
      } else {
	int on = 1;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0)
	  throw SystemCallException("setsockopt(SO_REUSEADDR)", errno);

	if (bind(fd, result->ai_addr, result->ai_addrlen) < 0)
	  if (autoPort)
	    continue;
	  else {
	    throw SystemCallException("bind", errno);
	  }

	if (protocol == TCP) {
	  listen_sk = fd;
	  fd	= -1;

	  if (listen(listen_sk, 5) < 0)
	    if (autoPort)
	      continue;
	    else
	      throw SystemCallException("listen", errno);

	  if (doAccept)
	    accept(deadline);
	  else
	    break;
	}
      }

      // we have an fd! break out of the infinite loop
      break;
    } catch (...) {
      if (listen_sk >= 0)
	close(listen_sk);

      throw;
    }
  }
}


SocketStream::~SocketStream() noexcept(false)
{
  //ScopedDelayCancellation dc; // close() can throw as it is a cancellation point

  if (listen_sk >= 0 && close(listen_sk) < 0 && !std::uncaught_exception())
    throw SystemCallException("close listen_sk", errno);
}


FileDescriptorBasedStream *SocketStream::detach()
{
  assert(mode == Server);

  FileDescriptorBasedStream *client = new FileDescriptorBasedStream(fd);

  fd = -1;

  return client;
}


void SocketStream::reaccept(time_t deadline)
{
  assert(mode == Server);

  if (fd >= 0 && close(fd) < 0)
    throw SystemCallException("close", errno);

  accept(deadline);
}


void SocketStream::accept(time_t deadline)
{
  if (nfskey != "")
    writekey(nfskey, port);

  // make sure the key will be deleted
  struct D {
    ~D() noexcept(false) {
      if (nfskey != "") {
        //ScopedDelayCancellation dc; // unlink is a cancellation point

        bool allowException = !std::uncaught_exception();

	try {
	  deletekey(nfskey);
	} catch (...) {
	  if (allowException)
	    throw;
	}
      }  
    }

    const std::string &nfskey;
  } onDestruct = { nfskey };
  (void) onDestruct;

  if (deadline > 0) {
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(listen_sk, &fds);

    struct timeval timeval;

    time_t now = time(0);
    
    if (now > deadline)
      throw TimeOutException("server socket");

    timeval.tv_sec  = deadline - now;
    timeval.tv_usec = 0;

    switch (select(listen_sk + 1, &fds, 0, 0, &timeval)) {
      case -1 : throw SystemCallException("select", errno);

      case  0 : throw TimeOutException("server socket");
    }
  }

  if ((fd = ::accept(listen_sk, 0, 0)) < 0)
    throw SystemCallException("accept", errno);
}


void SocketStream::setReadBufferSize(size_t size)
{
  if (fd >= 0 && setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof size) < 0)
    perror("setsockopt failed");
}


void SocketStream::setWriteBufferSize(size_t size)
{
  if (fd >= 0 && setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof size) < 0)
    perror("setsockopt failed");
}


void SocketStream::setTimeout(double seconds)
{
  if (seconds >= 0) {
    struct timeval tv;
    tv.tv_sec  = static_cast<time_t>(seconds);
    tv.tv_usec = static_cast<suseconds_t>((seconds - floor(seconds)) * 10e6);

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0 || setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv) < 0)
      throw SystemCallException("setsockopt", errno);
  }
}


void SocketStream::syncNFS()
{
  // sync NFS
  DIR *dir = opendir(".");

  if (!dir)
    throw SystemCallException("opendir", errno);

  if (!readdir(dir))
    throw SystemCallException("readdir", errno);

  if (closedir(dir) != 0)
    throw SystemCallException("closedir", errno);
}


std::string SocketStream::readkey(const std::string &nfskey, time_t deadline)
{
  for(;;) {
    char portStr[16];
    ssize_t len;

    syncNFS();

    len = readlink(nfskey.c_str(), portStr, sizeof portStr - 1); // reserve 1 character to insert \0 below

    if (len >= 0) {
      portStr[len] = 0;
      return std::string(portStr);
    }

    if (deadline > 0 && deadline <= time(0))
      throw TimeOutException("client socket");

    if (usleep(999999) > 0) {
      // interrupted by a signal handler -- abort to allow this thread to
      // be forced to continue after receiving a SIGINT, as with any other
      // system call
      throw SystemCallException("sleep", errno);
    }
  }
}

void SocketStream::writekey(const std::string &nfskey, uint16_t port)
{
  char portStr[16];

  snprintf(portStr, sizeof portStr, "%hu", port);

  // Symlinks can be atomically created over NFS
  if (symlink(portStr, nfskey.c_str()) < 0)
    throw SystemCallException("symlink", errno);
}


void SocketStream::deletekey(const std::string &nfskey)
{
  syncNFS();

  if (unlink(nfskey.c_str()) < 0)
    throw SystemCallException("unlink", errno);
}
