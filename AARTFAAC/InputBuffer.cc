#include "Common/Config.h"

#include "AARTFAAC/InputBuffer.h"
#include "Common/Affinity.h"
#include "Common/Stream/Descriptor.h"
#include "Common/Stream/SocketStream.h"

#include <byteswap.h>
#include <omp.h>
#include <sys/socket.h>

#if defined __AVX__
#include <immintrin.h>
#endif

#include <cassert>
#include <chrono>

#undef FAKE_TIMES
#undef USE_RECVMMSG



volatile std::sig_atomic_t InputBuffer::signalCaught = false;


void uncached_memcpy(void *__restrict dst, const void *__restrict src, size_t size)
{
#if defined __AVX__
  size_t done = 0;

#if 0
  // size should be multiple of 2

  while (done < size && ((ptrdiff_t) ((char *) dst + done) & 0x1F) != 0) {
    * (short *) ((char *) dst + done) = * (short *) ((const char *) src + done);
    done += sizeof(short);
  }
#endif

  while (done + sizeof(__m256i) <= size) {
    _mm256_stream_si256((__m256i *) ((char *) dst + done), _mm256_loadu_si256((const __m256i *) ((const char *) src + done)));
    done += sizeof(__m256i);
  }

#if 0
  while (done < size) {
    * (short *) ((char *) dst + done) = * (short *) ((const char *) src + done);
    done += sizeof(short);
  }
#endif
#else
  memcpy(dst, src, size);
#endif
}


void uncached_memclear(void *dst, size_t size)
{
#if defined __AVX__
  size_t done = 0;

#if 0
  // size should be multiple of sizeof(int)

  while (done < size && ((ptrdiff_t) ((char *) dst + done) & 0x1F) != 0) {
    * (int *) ((char *) dst + done) = 0;
    done += sizeof(int);
  }
#endif

  while (done + sizeof(__m256i) <= size) {
    _mm256_stream_si256((__m256i *) ((char *) dst + done), _mm256_setzero_si256());
    done += sizeof(__m256i);
  }

#if 0
  while (done < size) {
    * (int *) ((char *) dst + done) = 0;
    done += sizeof(int);
  }
#endif
#else
  memset(dst, 0, size);
#endif
}


InputBuffer::InputBuffer(const AARTFAAC_Parset &ps, MultiArrayHostBuffer<char, 4> hostRingBuffer[], unsigned myFirstSubband, unsigned myNrSubbands, unsigned myFirstStation, unsigned myNrStations, unsigned nrTimesPerPacket)
:
  ps(ps),
  myFirstSubband(myFirstSubband),
  myNrSubbands(myNrSubbands),
  myFirstStation(myFirstStation),
  myNrStations(myNrStations),
  nrRingBufferSamplesPerSubband(ps.nrRingBufferSamplesPerSubband()),
  hostRingBuffer(hostRingBuffer),
  nrTimesPerPacket(nrTimesPerPacket),
  nrHistorySamples((NR_TAPS - 1) * ps.nrChannelsPerSubband()),
  latestWriteTime(0, ps.clockSpeed()),
  stop(false),
  readerAndWriterSynchronization(nrRingBufferSamplesPerSubband, ps.startTime() - nrHistorySamples - 20),
  inputThread(&InputBuffer::inputThreadBody, this),
  logThread(&InputBuffer::logThreadBody, this),
  noInputThreadPtr(ps.realTime() ? new std::thread(&InputBuffer::noInputThreadBody, this) : 0)
{
#if defined __AVX__
  // check alignment for _mm256_stream_si256
  assert((ptrdiff_t) hostRingBuffer[0].origin() % 32 == 0);
  assert(myNrStations * ps.nrPolarizations() * ps.nrBytesPerComplexSample() % 32 == 0);
#endif

//#pragma omp critical (clog)
  //std::clog << logMessage() << " created by CPU " << currentCPU() << " on node " << currentNode() << ", memory at node " << node(ringBuffer.data()) << std::endl;
}


InputBuffer::~InputBuffer()
{
  //readerAndWriterSynchronization.noMoreReading(); // FIXME: not here
  stop = true;

  if (noInputThreadPtr.get() != nullptr)
    noInputThreadPtr->join();

  logThread.join();
  inputThread.join();
}


std::ostream &operator << (std::ostream &os, std::function<std::ostream & (std::ostream &os)> function)
{
  return function(os);
}


std::function<std::ostream & (std::ostream &)> InputBuffer::logMessage() const
{
  return [&] (std::ostream &os) -> std::ostream & { return os << "InputBuffer " << myFirstStation << '-' << myFirstStation + myNrStations - 1; };
}


void InputBuffer::logThreadBody()
{
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
  try {
#endif
    while (!stop && !signalCaught) {
      std::this_thread::sleep_for(std::chrono::seconds(1));

      std::lock_guard<std::mutex> lock(validDataMutex);
#pragma omp critical (clog)
      std::clog << logMessage() << ", valid: " << validData << std::endl;
    }
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
  } catch (std::exception &ex) {
#pragma omp critical (cerr)
    std::cerr << logMessage() << "caught std::exception: " << ex.what() << std::endl;
  }
#endif
}


struct Header
{
  uint64_t rsp_lane_id : 2;
  uint64_t rsp_sdo_mode : 2;
  uint64_t rsp_rsp_clock : 1;
  uint64_t rsp_reserved_1 : 59;

  uint16_t rsp_station_id;
  uint16_t nof_words_per_block;
  uint16_t nof_blocks_per_packet;

  uint64_t rsp_bsn /*: 50;
  uint64_t rsp_reserved_0 : 13;
  uint64_t rsp_sync : 1*/;

  //uint8_t  pad[8];
  //uint64_t block_seqno;

  TimeStamp timeStamp(unsigned clockSpeed)
  {
#if defined __BIG_ENDIAN__
    return TimeStamp(rsp_bsn & 0x3FFFFFFFFFFFFULL, clockSpeed);
#else
    return TimeStamp(__bswap_64(rsp_bsn) & 0x3FFFFFFFFFFFFULL, clockSpeed);
#endif
  }
} __attribute__((packed));


inline std::complex<int16_t> bswap(std::complex<int16_t> z)
{
  return std::complex<int16_t>(__bswap_16(real(z)), __bswap_16(imag(z)));
}


inline std::complex<int8_t> bswap(std::complex<int8_t> z)
{
  return z;
}


inline int8_t bswap(int8_t z)
{
  return z;
}


void InputBuffer::handleConsecutivePackets(char packetBuffer[maxNrPacketsInBuffer][maxPacketSize], unsigned firstPacket, unsigned lastPacket)
{
  Header    *hdr      = (Header *) packetBuffer[firstPacket];
  TimeStamp beginTime = hdr->timeStamp(ps.clockSpeed());

  std::lock_guard<std::mutex> latestWriteTimeLock(latestWriteTimeMutex);

  if (beginTime >= latestWriteTime) {
    unsigned timeIndex = beginTime % nrRingBufferSamplesPerSubband;
    unsigned myNrTimes = (lastPacket - firstPacket) * nrTimesPerPacket;
    TimeStamp endTime(beginTime + myNrTimes);

    latestWriteTime = endTime;
    readerAndWriterSynchronization.startWrite(beginTime, endTime);

    size_t size = myNrStations * ps.nrPolarizations() * ps.nrBytesPerComplexSample();

    for (unsigned packet = firstPacket; packet < lastPacket; packet ++) {
      const char *payLoad = &packetBuffer[packet][sizeof(Header)];

      for (unsigned time = 0; time < nrTimesPerPacket; time ++) {
	for (unsigned subband = 0; subband < myNrSubbands; subband ++) {
	  const char *readPtr = &payLoad[(time * myNrSubbands + subband) * size];
	  char *writePtr = hostRingBuffer[subband][timeIndex][myFirstStation].origin();

	  //assert(writePtr + size <= hostRingBuffer.origin() + hostRingBuffer.num_elements());
	  uncached_memcpy(writePtr, readPtr, size);
	}

	if (++ timeIndex == nrRingBufferSamplesPerSubband)
	  timeIndex = 0;
      }
    }

    {
      std::lock_guard<std::mutex> lock(validDataMutex);
      validData.exclude(TimeStamp(0, ps.clockSpeed()), endTime - nrRingBufferSamplesPerSubband);
      const SparseSet<TimeStamp>::Ranges &ranges = validData.getRanges();

      if (ranges.size() < 16 || ranges.back().end == beginTime)
	validData.include(beginTime, endTime);
    }

    readerAndWriterSynchronization.finishedWrite(endTime);
  }
}


void InputBuffer::inputThreadBody()
{
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
  try {
#endif

#if 0 && defined __linux__
    setScheduler(SCHED_FIFO, 99);
#endif

    std::unique_ptr<Stream> stream(createStream(ps.inputDescriptors()[myFirstStation / myNrStations], true));

    SocketStream *socketStream = dynamic_cast<SocketStream *>(stream.get());

    if (socketStream != nullptr) {
      socketStream->setReadBufferSize(127 * 1024 * 1024);
      socketStream->setTimeout(1);
    }

#if defined USE_RECVMMSG
    FileDescriptorBasedStream *fdStream = dynamic_cast<FileDescriptorBasedStream *>(stream.get());
    assert(fdStream != nullptr);
#endif

    TimeStamp expectedTimeStamp(0, ps.clockSpeed()), stopTime = ps.stopTime() + ps.nrSamplesPerSubband();

#if defined FAKE_TIMES
    //expectedTimeStamp = ps.startTime() - nrHistorySamples - 20;
    expectedTimeStamp = TimeStamp::now(ps.clockSpeed()) - nrHistorySamples - 20;
#endif

    size_t packetSize = sizeof(Header) + myNrSubbands * myNrStations * ps.nrPolarizations() * nrTimesPerPacket * ps.nrBytesPerComplexSample();
    assert(packetSize <= maxPacketSize);
    bool printedImpossibleTimeStampWarning = false;

    char packetBuffer[maxNrPacketsInBuffer][maxPacketSize] __attribute__((aligned(16)));
    unsigned nrPackets, firstPacket, nextPacket;
    TimeStamp timeStamp;

#if defined USE_RECVMMSG
    struct iovec   iovecs[maxNrPacketsInBuffer];
    struct mmsghdr msgvec[maxNrPacketsInBuffer];

    for (int packet = 0; packet < maxNrPacketsInBuffer; packet ++) {
      iovecs[packet].iov_base		  = packetBuffer[packet];
      iovecs[packet].iov_len		  = maxPacketSize;
      msgvec[packet].msg_hdr.msg_iov    = &iovecs[packet];
      msgvec[packet].msg_hdr.msg_iovlen = 1;
    }
#endif

    do {
      try {
	// receive packets

#if defined USE_RECVMMSG
	nrPackets = recvmmsg(fdStream->fd, msgvec, maxNrPacketsInBuffer, 0, nullptr);

	if ((int) nrPackets < 0)
	  throw SystemCallException("recvmmsg");
	else if (nrPackets == 0)
	  throw Stream::EndOfStreamException("");
#else
	for (nrPackets = 0; nrPackets < maxNrPacketsInBuffer; nrPackets ++)
	  stream->read(packetBuffer[nrPackets], packetSize);
#endif
      } catch (Stream::EndOfStreamException) {
#pragma omp critical (clog)
	std::clog << logMessage() << " caught EndOfStreamException" << std::endl;
	stop = true;
      } catch (const SystemCallException &ex) {
	if (ex.error != EAGAIN && ex.error != EINTR) // expect timeout; rethrow others
	  throw;
      }

      // handle packets

      for (firstPacket = nextPacket = 0; nextPacket < nrPackets; nextPacket ++) {
	Header *hdr = reinterpret_cast<Header *>(packetBuffer[nextPacket]);

#if defined FAKE_TIMES
	hdr->block_seqno = __bswap_64((int64_t) (expectedTimeStamp));
#endif
	timeStamp = hdr->timeStamp(ps.clockSpeed());

	if (timeStamp != expectedTimeStamp) {
	  if (firstPacket < nextPacket)
	    handleConsecutivePackets(packetBuffer, firstPacket, nextPacket);

	  if (ps.realTime() && abs(TimeStamp::now(ps.clockSpeed()) - timeStamp) > 15 * ps.subbandBandwidth()) {
	    if (!printedImpossibleTimeStampWarning) {
	      printedImpossibleTimeStampWarning = true;
#pragma omp critical (clog)
	      std::clog << logMessage() << ": impossible timestamp " << timeStamp << std::endl;
	    }

	    firstPacket = nextPacket + 1; // reject this packet
	    timeStamp = 0;
	  } else {
	    printedImpossibleTimeStampWarning = false;
	  }
	}

	expectedTimeStamp = timeStamp + nrTimesPerPacket;
      }

      if (firstPacket < nextPacket)
	handleConsecutivePackets(packetBuffer, firstPacket, nextPacket);
    } while (timeStamp < stopTime && !stop && !signalCaught);

    readerAndWriterSynchronization.noMoreWriting();
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
  } catch (std::exception &ex) {
    readerAndWriterSynchronization.noMoreWriting();

#pragma omp critical (cerr)
    std::cerr << logMessage() << " caught std::exception: " << ex.what() << std::endl;
  }
#endif
}


void InputBuffer::noInputThreadBody()
{
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
  try {
#endif
    // forces the correlator to proceed even if no data is received

    WallClockTime wallClock;
    bool lateLastTime = false;

    for (TimeStamp timeStamp = ps.startTime(); timeStamp < ps.stopTime() + ps.nrSamplesPerSubband() * ps.visibilitiesIntegration() && !stop && !signalCaught; timeStamp += ps.subbandBandwidth() / 10) {
      wallClock.waitUntil(timeStamp + ps.subbandBandwidth() / 3);

      std::lock_guard<std::mutex> lock(latestWriteTimeMutex);

      if (latestWriteTime < timeStamp) {
	readerAndWriterSynchronization.startWrite(latestWriteTime, timeStamp);
	readerAndWriterSynchronization.finishedWrite(timeStamp);
	latestWriteTime = timeStamp;

	if (!lateLastTime) {
#pragma omp critical (clog)
	  std::clog << logMessage() << ": forcing correlator to continue without data " << timeStamp << std::endl;
	  lateLastTime = true;
	}
      } else if (lateLastTime) {
#pragma omp critical (clog)
	std::clog << logMessage() << ": resumed normal operation " << timeStamp << std::endl;
	lateLastTime = false;
      }
    }

    readerAndWriterSynchronization.noMoreWriting();
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
  } catch (std::exception &ex) {
#pragma omp critical (cerr)
    std::cerr << logMessage() << " caught std::exception: " << ex.what() << std::endl;

    readerAndWriterSynchronization.noMoreWriting();
  }
#endif
}


SparseSet<TimeStamp> InputBuffer::getCurrentValidData(const TimeStamp &earlyStartTime, const TimeStamp &endTime)
{
  std::lock_guard<std::mutex> lock(validDataMutex);
  return validData & SparseSet<TimeStamp>(earlyStartTime, endTime);
}


void InputBuffer::fillInMissingSamples(const TimeStamp &startTime, unsigned subband, SparseSet<TimeStamp> &validData)
{
  TimeStamp earlyStartTime   = startTime - nrHistorySamples;
  TimeStamp endTime          = startTime + ps.nrSamplesPerSubband();

  validData = getCurrentValidData(earlyStartTime, endTime);
  SparseSet<TimeStamp> flaggedData = validData.invert(earlyStartTime, endTime);
  const SparseSet<TimeStamp>::Ranges &flaggedRanges = flaggedData.getRanges();
  size_t size = myNrStations * ps.nrPolarizations() * ps.nrBytesPerComplexSample();

  for (const SparseSet<TimeStamp>::range &it : flaggedRanges) {
    for (unsigned timeIndex = it.begin % nrRingBufferSamplesPerSubband, timeEndIndex = it.end % nrRingBufferSamplesPerSubband; timeIndex != timeEndIndex;) {
      uncached_memclear(hostRingBuffer[subband][timeIndex][myFirstStation].origin(), size);

      if (++ timeIndex == nrRingBufferSamplesPerSubband)
	timeIndex = 0;
    }
  }

  unsigned nrHistorySamples = (NR_TAPS - 1) * ps.nrChannelsPerSubband();
  unsigned nrSamples        = nrHistorySamples + ps.nrSamplesPerSubband();

  if (subband == 0)
#pragma omp critical (clog)
    std::clog << logMessage() << ' ' << earlyStartTime << " flagged: " << 100.0 * (int64_t) flaggedData.count() / nrSamples << "% (" << flaggedRanges.size() << ')' << std::endl;
}


void InputBuffer::startReadTransaction(const TimeStamp &startTime)
{
  TimeStamp earlyStartTime   = startTime - nrHistorySamples;
  TimeStamp endTime          = startTime + ps.nrSamplesPerSubband();

  readerAndWriterSynchronization.startRead(earlyStartTime, endTime);
}


void InputBuffer::endReadTransaction(const TimeStamp &startTime)
{
  TimeStamp endTime          = startTime + ps.nrSamplesPerSubband();

  readerAndWriterSynchronization.finishedRead(endTime - nrHistorySamples - 20);
}


void InputBuffer::caughtSignal()
{
  signalCaught = true;
}
