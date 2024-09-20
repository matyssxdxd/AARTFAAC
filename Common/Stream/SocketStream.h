#ifndef STREAM_SOCKET_STREAM_H
#define STREAM_SOCKET_STREAM_H

#include "Common/Exceptions/Exception.h"
#include "Common/SystemCallException.h"
#include "Common/Stream/FileDescriptorBasedStream.h"

#include <stdint.h>
#include <string>
#include <time.h>


class SocketStream : public FileDescriptorBasedStream
{
  public:
    enum Protocol {
      TCP, UDP
    };

    enum Mode {
      Client, Server
    };

    class TimeOutException : public Exception
    {
      public:
        TimeOutException(const std::string &msg);
    };

    SocketStream(const std::string &hostname, uint16_t _port, Protocol, Mode, time_t deadline = 0, const std::string &nfskey = "", bool doAccept = true);
    virtual ~SocketStream() noexcept(false);

    FileDescriptorBasedStream *detach();

    void  reaccept(time_t deadline = 0); // only for TCP server socket
    void  setReadBufferSize(size_t size);
    void  setWriteBufferSize(size_t size);
    void  setTimeout(double seconds);

    const Protocol protocol;
    const Mode mode;

  private:
    const std::string hostname;
    uint16_t port;
    const std::string nfskey;
    int listen_sk;

    void accept(time_t timeout);

    static void syncNFS();

    static std::string readkey(const std::string &nfskey, time_t deadline);
    static void writekey(const std::string &nfskey, uint16_t port);
    static void deletekey(const std::string &nfskey);
};

#endif
