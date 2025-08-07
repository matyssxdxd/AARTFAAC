#include "Common/Config.h"

#include "Common/Affinity.h"
#include "ISBI/InputBuffer.h"
#include "ISBI/InputSection.h"

#include <fstream>


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





void InputSection::enqueueHostToDeviceCopy(cu::Stream &stream, cu::DeviceMemory &devBuffer, PerformanceCounter &counter, const TimeStamp &startTime, unsigned subband)
{
  uint64_t timeOffset = startTime - ps.startTime();
  uint64_t totalTimeRange = ps.stopTime() - ps.startTime();
  double proportion = static_cast<double>(timeOffset) / totalTimeRange;
  int delayTimeIndex = std::min(static_cast<int>(proportion * ps.trueDelays().size() / 2), static_cast<int>(ps.trueDelays().size() / 2 - 1));
  
  for (unsigned station = 0; station < ps.nrStations(); station++) {
    int delay = ps.trueDelays()[station * ps.trueDelays().size() / 2 + delayTimeIndex];
    unsigned nrHistorySamples = (NR_TAPS - 1) * ps.nrChannelsPerSubband();
    TimeStamp earlyStartTime   = startTime - nrHistorySamples + delay;
    TimeStamp endTime          = startTime + ps.nrSamplesPerSubbandBeforeFilter();

    unsigned startTimeIndex = earlyStartTime % ps.nrRingBufferSamplesPerSubband();
    unsigned endTimeIndex = endTime % ps.nrRingBufferSamplesPerSubband();

    size_t nrBytesPerTime = ps.nrBytesPerRealSample();

#if 0
    for (unsigned time = startTimeIndex; time != endTimeIndex; time ++, time %= ps.nrRingBufferSamplesPerSubband())
      for (unsigned station = 0; station < ps.nrStations(); station ++)
        for (unsigned polarization = 0; polarization < ps.nrPolarizations(); polarization ++)
          switch (ps.nrBitsPerSample()) {
            case 16 : * ((std::complex<short> *) hostRingBuffers[subband][time][station][polarization].origin()) = std::complex<short>(0);
  
  		    if (time == (startTimeIndex + 0) % ps.nrRingBufferSamplesPerSubband() && station == 42 && polarization == 0)
  		      * ((std::complex<short> *) hostRingBuffers[subband][time][station][polarization].origin()) = std::complex<short>(128, 0);
  		    if (time == (startTimeIndex + 0) % ps.nrRingBufferSamplesPerSubband() && station == 43 && polarization == 1)
  		      * ((std::complex<short> *) hostRingBuffers[subband][time][station][polarization].origin()) = std::complex<short>(42, 42);
  
  		    break;
          }
#endif

    {
      PerformanceCounter::Measurement measurement(counter, stream, 0, 0, (endTime - earlyStartTime) * nrBytesPerTime);

      uint32_t n = endTime - earlyStartTime;

      assert(n <= ps.nrRingBufferSamplesPerSubband());

      for (unsigned pol = 0; pol < ps.nrPolarizations(); pol++) {
        size_t offset = (station * ps.nrPolarizations() + pol) * n * nrBytesPerTime;

        uint32_t firstPart = ps.nrRingBufferSamplesPerSubband() - startTimeIndex;
        uint32_t secondPart = 0; 

        if (startTimeIndex < endTimeIndex) {
          firstPart = endTimeIndex - startTimeIndex;
        } else {
          secondPart = n - firstPart;
        }

        assert(firstPart + secondPart == n);

#if 0
        std::cout << "startTimeIndex: " << startTimeIndex << "\n"; 
        std::cout << "endTimeIndex: " << endTimeIndex << "\n"; 
        std::cout << "n: " << n << "\n";
        std::cout << "firstPart: " << firstPart << "\n";
        std::cout << "secondPart: " << secondPart << "\n";
        std::cout << firstPart * nrBytesPerTime << " bytes to copy.\n";
        std::cout << secondPart * nrBytesPerTime << " bytes to copy. (second part)\n";
#endif    

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

