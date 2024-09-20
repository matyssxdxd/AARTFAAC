#include "Common/Config.h"

#include "Common/ReaderWriterSynchronization.h"


ReaderAndWriterSynchronization::~ReaderAndWriterSynchronization()
{
}


void ReaderAndWriterSynchronization::noMoreReading()
{
}


void ReaderAndWriterSynchronization::noMoreWriting()
{
}


SynchronizedReaderAndWriter::SynchronizedReaderAndWriter(unsigned bufferSize, const TimeStamp &initialValue)
:
  itsReadPointer(initialValue),
  itsBufferSize(bufferSize)
{
}


SynchronizedReaderAndWriter::~SynchronizedReaderAndWriter()
{
}


void SynchronizedReaderAndWriter::startRead(const TimeStamp &begin, const TimeStamp &end)
{
  itsReadPointer.advanceTo(begin);
  itsWritePointer.waitFor(end);
}


void SynchronizedReaderAndWriter::finishedRead(const TimeStamp &advanceTo)
{
  itsReadPointer.advanceTo(advanceTo);
}


void SynchronizedReaderAndWriter::startWrite(const TimeStamp &begin, const TimeStamp &end)
{
  itsWritePointer.advanceTo(begin); // avoid deadlock if there is a gap in the written data
  itsReadPointer.waitFor(end - itsBufferSize);
}


void SynchronizedReaderAndWriter::finishedWrite(const TimeStamp &advanceTo)
{
  itsWritePointer.advanceTo(advanceTo);
}


void SynchronizedReaderAndWriter::noMoreReading()
{
  // advance read pointer to infinity, to unblock thread that waits in startWrite
  itsReadPointer.advanceTo(TimeStamp(0x7FFFFFFFFFFFFFFFLL, 0)); // we only use this TimeStamp for comparison so clockSpeed does not matter
}


void SynchronizedReaderAndWriter::noMoreWriting()
{
  itsWritePointer.advanceTo(TimeStamp(0x7FFFFFFFFFFFFFFFLL, 0));
}


TimeSynchronizedReader::TimeSynchronizedReader(unsigned maximumNetworkLatency)
:
  itsMaximumNetworkLatency(maximumNetworkLatency)
{
}


TimeSynchronizedReader::~TimeSynchronizedReader()
{
}


void TimeSynchronizedReader::startRead(const TimeStamp & /*begin*/, const TimeStamp &end)
{
  itsWallClock.waitUntil(end + itsMaximumNetworkLatency);
}


void TimeSynchronizedReader::finishedRead(const TimeStamp & /*advanceTo*/)
{
}


void TimeSynchronizedReader::startWrite(const TimeStamp & /*begin*/, const TimeStamp & /*end*/)
{
}


void TimeSynchronizedReader::finishedWrite(const TimeStamp & /*advanceTo*/)
{
}
