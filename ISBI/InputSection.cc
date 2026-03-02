#include "Common/Config.h"

#include "Common/Affinity.h"
#include "ISBI/InputBuffer.h"
#include "ISBI/InputSection.h"

#include <fstream>

#define ISBI_DELAYS

InputSection::InputSection(const ISBI_Parset &ps)
:
  ps(ps),

  hostRingBuffers([&] () {
    std::vector<MultiArrayHostBuffer<char, 4>> buffers; 

    for (unsigned subband = 0; subband < ps.nrSubbands(); subband ++)
        buffers.emplace_back(std::move(boost::extents[ps.nrStations()][ps.nrPolarizations()][ps.nrRingBufferSamplesPerSubband()][ps.nrBytesPerRealSample()]), CU_MEMHOSTALLOC_WRITECOMBINED);

    return std::move(buffers);
  } ()),

  inputBuffers([&] () {
    std::vector<std::unique_ptr<InputBuffer>> buffers;

    for (unsigned stationSet = 0; stationSet < ps.inputDescriptors().size(); stationSet ++) {
      std::unique_ptr<BoundThread> bt(ps.inputBufferNodes().size() > 0 ? new BoundThread(ps.allowedCPUs(ps.inputBufferNodes()[stationSet])) : nullptr);
      buffers.emplace_back(new InputBuffer(ps, &hostRingBuffers[0], 1, ps.nrSubbands(), stationSet, 1, nrTimesPerPacket));
    }

    return std::move(buffers);
  } ())
{
}



InputSection::~InputSection()
{
#pragma omp parallel for
  for (unsigned i = 0; i < inputBuffers.size(); i ++)
    inputBuffers[i] = nullptr;
}




void InputSection::enqueueHostToDeviceCopy(cu::Stream &stream, cu::DeviceMemory &devBuffer, PerformanceCounter &counter, const TimeStamp &startTime, unsigned subband) {
  std::clog << "-== InputSection ==-" << std::endl;

  uint32_t blockSize = ps.nrSamplesPerSubbandBeforeFilter();
  uint64_t timeOffset = startTime - ps.startTime();
  uint64_t totalTimeRange = ps.stopTime() - ps.startTime();
  uint32_t N = static_cast<uint32_t>(totalTimeRange / blockSize);
  uint32_t delayIndex = std::min(static_cast<uint32_t>((timeOffset / blockSize)), N + 1);

  std::size_t delaySize = ps.delays().size();

  if (delaySize < N) {
    std::clog << "WARNING: There are in total " << N
              << " blocks, but only " << delaySize 
              << " delays were provided!" << std::endl;
  }

  for (unsigned station = 0; station < ps.nrStations(); station++) {
    int delay = 0;
    if (station == 1) {
      double delayInSamples = ps.delays()[delayIndex] * ps.sampleRate();
      delay = static_cast<int>(std::floor(delayInSamples + 0.5));
    }

    unsigned nrHistorySamples = (NR_TAPS - 1) * ps.nrChannelsPerSubbandBeforeFilter();

    TimeStamp earlyStartTime   = startTime - nrHistorySamples + delay;
    TimeStamp endTime          = startTime + ps.nrSamplesPerSubbandBeforeFilter() + delay;

    unsigned startTimeIndex = earlyStartTime % ps.nrRingBufferSamplesPerSubband();
    unsigned endTimeIndex = endTime % ps.nrRingBufferSamplesPerSubband();

    std::cout << "block=" << delayIndex << std::endl;
    std::cout << "startTimeIndex=" << startTimeIndex << std::endl;
    std::cout << "endTimeIndex=" << endTimeIndex << std::endl;

    unsigned nrBytesPerTime = ps.nrBytesPerRealSample();

    {
      PerformanceCounter::Measurement measurement(counter, stream, 0, 0, (endTime - earlyStartTime) * nrBytesPerTime);

      uint32_t n = endTime - earlyStartTime;
      assert(n <= ps.nrRingBufferSamplesPerSubband());

      for (unsigned pol = 0; pol < ps.nrPolarizations(); pol++) {
        size_t offset = (station * ps.nrPolarizations() + pol) * n * nrBytesPerTime;

        std::cout << "station=" << station << std::endl;
        std::cout << "delay=" << delay << std::endl;
        std::cout << "pol=" << pol << std::endl;
        std::cout << "offset=" << offset << std::endl;

        uint32_t firstPart = ps.nrRingBufferSamplesPerSubband() - startTimeIndex;
        uint32_t secondPart = 0; 

        if (startTimeIndex < endTimeIndex) {
          firstPart = endTimeIndex - startTimeIndex;
        } else {
          secondPart = n - firstPart;
        }

        std::cout << "firstPart=" << firstPart << std::endl;
        std::cout << "secondPart=" << secondPart << std::endl;
        std::cout << "firstPart+secondPart=" << firstPart + secondPart << std::endl;
        std::cout << "n=" << n << std::endl;

        assert(firstPart + secondPart == n);

        if (firstPart > 0) {
          cu::DeviceMemory dst(devBuffer + offset);
          stream.memcpyHtoDAsync(
              dst, 
              hostRingBuffers[subband][station][pol][startTimeIndex].origin(),
              firstPart * nrBytesPerTime
              );
        }

        if (secondPart > 0) {
          cu::DeviceMemory dst(devBuffer + offset + firstPart * nrBytesPerTime);
          stream.memcpyHtoDAsync(
              dst,
              hostRingBuffers[subband][station][pol][0].origin(),
              secondPart * nrBytesPerTime
              );
        }
      }
    }
  }
#if 0
  char filename[1024];
  sprintf(filename, "/var/scratch/romein/out.%u", subband + 8);
  std::ofstream file(filename, std::ios::binary | std::ios::app);

  for (TimeStamp time = startTime; time < endTime; time ++)
    file.write(hostRingBuffers[subband][time % ps.nrRingBufferSamplesPerSubband()].origin(), ps.nrStations() * ps.nrPolarizations() * ps.nrBytesPerComplexSample());
#endif
}



void InputSection::fillInMissingSamples(const TimeStamp &time, unsigned subband, std::vector<SparseSet<TimeStamp> > &validData)
{
  for (unsigned stationSet = 0; stationSet < inputBuffers.size(); stationSet ++)
    inputBuffers[stationSet]->fillInMissingSamples(time, subband, validData[stationSet]);
}


void InputSection::startReadTransaction(const TimeStamp &time)
{
  for (std::unique_ptr<InputBuffer> &inputBuffer : inputBuffers)
    inputBuffer->startReadTransaction(time);
}


void InputSection::endReadTransaction(const TimeStamp &time)
{
  for (std::unique_ptr<InputBuffer> &inputBuffer : inputBuffers)
    inputBuffer->endReadTransaction(time);
}

