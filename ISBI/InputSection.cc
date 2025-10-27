#include "Common/Config.h"

#include "Common/Affinity.h"
#include "ISBI/InputBuffer.h"
#include "ISBI/InputSection.h"

#include <fstream>

#undef ISBI_DELAYS

InputSection::InputSection(const ISBI_Parset &ps)
:
  ps(ps),

  hostRingBuffers([&] () {
    std::vector<MultiArrayHostBuffer<char, 4>> buffers; 

    for (unsigned subband = 0; subband < ps.nrSubbands(); subband ++)
        buffers.emplace_back(std::move(boost::extents[ps.nrRingBufferSamplesPerSubband()][ps.nrStations()][ps.nrPolarizations()][ps.nrBytesPerRealSample()]), CU_MEMHOSTALLOC_WRITECOMBINED);

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

  unsigned nrHistorySamples = (NR_TAPS - 1) * ps.nrChannelsPerSubband();
  TimeStamp earlyStartTime   = startTime - nrHistorySamples;
  TimeStamp endTime          = startTime + ps.nrSamplesPerSubbandBeforeFilter();

  unsigned startTimeIndex = earlyStartTime % ps.nrRingBufferSamplesPerSubband();
  unsigned endTimeIndex = endTime % ps.nrRingBufferSamplesPerSubband();

  size_t nrBytesPerTime = ps.nrBytesPerRealSample();

  {
    PerformanceCounter::Measurement measurement(counter, stream, 0, 0, (endTime - earlyStartTime) * nrBytesPerTime);

    if (startTimeIndex < endTimeIndex) {
      stream.memcpyHtoDAsync(devBuffer, hostRingBuffers[subband][startTimeIndex].origin(), (endTimeIndex - startTimeIndex) * nrBytesPerTime);
    } else {
      stream.memcpyHtoDAsync(devBuffer, hostRingBuffers[subband][startTimeIndex].origin(), (ps.nrRingBufferSamplesPerSubband() - startTimeIndex) * nrBytesPerTime);

      if (endTimeIndex > 0) {
        cu::DeviceMemory dst(devBuffer + (ps.nrRingBufferSamplesPerSubband() - startTimeIndex) * nrBytesPerTime);
        stream.memcpyHtoDAsync(dst, hostRingBuffers[subband].origin(), endTimeIndex * nrBytesPerTime);
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

