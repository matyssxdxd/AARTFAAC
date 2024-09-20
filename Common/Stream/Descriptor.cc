#include "Common/Config.h"

#include "Common/Stream/Descriptor.h"
#include "Common/Stream/FileDescriptorBasedStream.h"
#include "Common/Stream/FileStream.h"
#include "Common/Stream/NamedPipeStream.h"
#include "Common/Stream/NullStream.h"
#include "Common/Stream/SocketStream.h"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <vector>


BadDescriptor::BadDescriptor(const std::string &descriptor)
:
  Exception(std::string("Bad descriptor: ") + descriptor)
{
}


Stream *createStream(const std::string &descriptor, bool asReader, time_t deadline)
{
  std::vector<std::string> split;
  boost::split(split, descriptor, boost::is_any_of(":"));

  if (descriptor == "null:")
    return new NullStream;
  else if (split.size() == 3 && split[0] == "udp")
    return new SocketStream(split[1].c_str(), boost::lexical_cast<unsigned short>(split[2]), SocketStream::UDP, asReader ? SocketStream::Server : SocketStream::Client, deadline);
  else if (split.size() == 3 && split[0] == "tcp")
    return new SocketStream(split[1].c_str(), boost::lexical_cast<unsigned short>(split[2]), SocketStream::TCP, asReader ? SocketStream::Server : SocketStream::Client, deadline);
  else if (split.size() == 3 && split[0] == "udpkey")
    return new SocketStream(split[1].c_str(), 0, SocketStream::UDP, asReader ? SocketStream::Server : SocketStream::Client, deadline, split[2].c_str());
  else if (split.size() == 3 && split[0] == "tcpkey")
    return new SocketStream(split[1].c_str(), 0, SocketStream::TCP, asReader ? SocketStream::Server : SocketStream::Client, deadline, split[2].c_str());
  else if (split.size() >= 2 && split[0] == "file")
    return asReader ? new FileStream(descriptor.c_str() + 5) : new FileStream(descriptor.c_str() + 5, 0666);
  else if (split.size() >= 2 && split[0] == "pipe")
    return new NamedPipeStream(descriptor.c_str() + 5, asReader);
  else if (split.size() >= 2 && split[0] == "fd")
    return new FileDescriptorBasedStream(boost::lexical_cast<int>(descriptor.c_str() + 3));
  else if (split.size() == 2)
    return new SocketStream(split[0].c_str(), boost::lexical_cast<unsigned short>(split[1]), SocketStream::UDP, asReader ? SocketStream::Server : SocketStream::Client, deadline);
  else if (split.size() == 1)
    return asReader ? new FileStream(split[0].c_str()) : new FileStream(split[0].c_str(), 0666);
  else
    throw BadDescriptor(descriptor);
}
