#ifndef COMMON_READER_WRITER_SYNCHRONIZATION
#define COMMON_READER_WRITER_SYNCHRONIZATION

#include "Common/TimeStamp.h"
#include "SlidingPointer.h"
#include "WallClockTime.h"


class ReaderAndWriterSynchronization
{
  public:
    virtual	 ~ReaderAndWriterSynchronization();

    virtual void startRead(const TimeStamp &begin, const TimeStamp &end) = 0;
    virtual void finishedRead(const TimeStamp &advanceTo) = 0;

    virtual void startWrite(const TimeStamp &begin, const TimeStamp &end) = 0;
    virtual void finishedWrite(const TimeStamp &advanceTo) = 0;

    virtual void noMoreReading();
    virtual void noMoreWriting();
};


class SynchronizedReaderAndWriter : public ReaderAndWriterSynchronization
{
  public:
		 SynchronizedReaderAndWriter(unsigned bufferSize, const TimeStamp &initialValue);
		 ~SynchronizedReaderAndWriter();

    virtual void startRead(const TimeStamp &begin, const TimeStamp &end);
    virtual void finishedRead(const TimeStamp &advanceTo);

    virtual void startWrite(const TimeStamp &begin, const TimeStamp &end);
    virtual void finishedWrite(const TimeStamp &advanceTo);

    virtual void noMoreReading();
    virtual void noMoreWriting();

  private:
    SlidingPointer<TimeStamp> itsReadPointer, itsWritePointer;
    unsigned		      itsBufferSize;
};


class TimeSynchronizedReader : public ReaderAndWriterSynchronization
{
  public:
		  TimeSynchronizedReader(unsigned maximumNetworkLatency);
		  ~TimeSynchronizedReader();

    virtual void  startRead(const TimeStamp &begin, const TimeStamp &end);
    virtual void  finishedRead(const TimeStamp &advanceTo);

    virtual void  startWrite(const TimeStamp &begin, const TimeStamp &end);
    virtual void  finishedWrite(const TimeStamp &advanceTo);
    
  private:
    WallClockTime itsWallClock;
    unsigned	  itsMaximumNetworkLatency;
};

#endif
