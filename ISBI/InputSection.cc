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
//  CHANGED
//      buffers.emplace_back(std::move(boost::extents[ps.nrRingBufferSamplesPerSubband()][ps.nrStations()][ps.nrPolarizations()][ps.nrBytesPerComplexSample()]), CU_MEMHOSTALLOC_WRITECOMBINED);
      
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
  unsigned nrHistorySamples = (NR_TAPS - 1) * ps.nrChannelsPerSubband();
  TimeStamp earlyStartTime   = startTime - nrHistorySamples;
  TimeStamp endTime          = startTime + ps.nrSamplesPerSubband();

  unsigned startTimeIndex = earlyStartTime % ps.nrRingBufferSamplesPerSubband();
  unsigned endTimeIndex = endTime % ps.nrRingBufferSamplesPerSubband();
// CHANGED
//  size_t nrBytesPerTime = ps.nrStations() * ps.nrPolarizations() * ps.nrBytesPerComplexSample();

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
    
    if (startTimeIndex < endTimeIndex) {
      for (unsigned station = 0; station < ps.nrStations(); station++) {
        for (unsigned pol = 0; pol < ps.nrPolarizations(); pol++) {
	  cu::DeviceMemory dst(devBuffer + (station * ps.nrPolarizations() + pol) * (endTimeIndex - startTimeIndex) * nrBytesPerTime);	
	  stream.memcpyHtoDAsync(dst, hostRingBuffers[subband][station][pol][startTimeIndex].origin(), (endTimeIndex - startTimeIndex) * nrBytesPerTime);
	}	
      }
    } else {
      for (unsigned station = 0; station < ps.nrStations(); station++) {
        for (unsigned pol = 0; pol < ps.nrPolarizations(); pol++) {
	  cu::DeviceMemory dst(devBuffer + (station * ps.nrPolarizations() + pol) * (ps.nrRingBufferSamplesPerSubband() - startTimeIndex) * nrBytesPerTime);
	  stream.memcpyHtoDAsync(dst, hostRingBuffers[subband][station][pol][startTimeIndex].origin(), (ps.nrRingBufferSamplesPerSubband() - startTimeIndex) * nrBytesPerTime);
	}
      }    

      if (endTimeIndex > 0) {
	for (unsigned station = 0; station < ps.nrStations(); station++) {
	  for (unsigned pol = 0; pol < ps.nrPolarizations(); pol++) {
		  cu::DeviceMemory dst(devBuffer + (ps.nrRingBufferSamplesPerSubband() - startTimeIndex) * nrBytesPerTime + (station * ps.nrPolarizations() + pol) * endTimeIndex * nrBytesPerTime);
		  stream.memcpyHtoDAsync(dst, hostRingBuffers[subband][station][pol].origin(), endTimeIndex * nrBytesPerTime);
	  }
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

