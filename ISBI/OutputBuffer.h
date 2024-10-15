#ifndef ISBI_OUTPUT_BUFFER_H
#define ISBI_OUTPUT_BUFFER_H

#include "ISBI/Parset.h"
#include "ISBI/Visibilities.h"
#include "Common/SlidingPointer.h"
#include "Common/Stream/Stream.h"
#include "Common/Threads/Queue.h"
#include "Common/TimeStamp.h"

#include <thread>


class OutputBuffer
{
  public:
    OutputBuffer(const ISBI_Parset &, unsigned subband);
    ~OutputBuffer();

    std::unique_ptr<Visibilities> getVisibilitiesBuffer();
    void putVisibilitiesBuffer(std::unique_ptr<Visibilities>, const TimeStamp &);

  private:
    void outputThreadBody();

    const ISBI_Parset	   	   &ps;
    const unsigned		   subband;
    SlidingPointer<TimeStamp>      currentTime;
    std::unique_ptr<Stream>	   stream;
    Queue<std::unique_ptr<Visibilities>> freeQueue, pendingQueue;

    std::thread thread;
};

#endif
