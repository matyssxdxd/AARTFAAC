#include "Common/Config.h"

#include "Common/BandPass.h"
#include "AARTFAAC/CorrelatorWorkQueue.h"

#include <iostream>


CorrelatorWorkQueue::CorrelatorWorkQueue(AARTFAAC_CorrelatorPipeline &pipeline, DeviceInstance &deviceInstance)
:
  ps(static_cast<const AARTFAAC_Parset &>(pipeline.ps)),
  pipeline(pipeline),
  deviceInstance(deviceInstance),

  hostDelays(boost::extents[ps.nrBeams()][ps.nrStations()][ps.nrPolarizations()]),

  validData(ps.inputDescriptors().size()) // FIXME???

#if defined USE_SEPARATE_THREAD
, stop(false),
  bufferEmpty(1),
  bufferFull(0)
#endif
{
  if (ps.delayCompensation()) {
    memset(hostDelays.origin(), 0, hostDelays.bytesize());
    //memset(phaseOffsets.origin(), 0, phaseOffsets.bytesize());

    if (ps.nrStations() == 288) {
      for (unsigned station = 0; station < 48; station ++) {
       hostDelays[0][station +  0 * 48][0] = 6.899769e-06;
       hostDelays[0][station +  0 * 48][1] = 6.900510e-06;
       hostDelays[0][station +  1 * 48][0] = 5.496243e-06;
       hostDelays[0][station +  1 * 48][1] = 5.495999e-06;
       hostDelays[0][station +  2 * 48][0] = 6.470178e-06;
       hostDelays[0][station +  2 * 48][1] = 6.470559e-06;
       hostDelays[0][station +  3 * 48][0] = 7.119765e-06;
       hostDelays[0][station +  3 * 48][1] = 7.120457e-06;
       hostDelays[0][station +  4 * 48][0] = 6.466346e-06;
       hostDelays[0][station +  4 * 48][1] = 6.467317e-06;
       hostDelays[0][station +  5 * 48][0] = 6.501126e-06;
       hostDelays[0][station +  5 * 48][1] = 6.501473e-06;
      }
    } else if (ps.nrStations() == 576) {
      for (unsigned station = 0; station < 48; station ++) {
       hostDelays[0][station +  0 * 48][0] = 6.899769e-06;
       hostDelays[0][station +  0 * 48][1] = 6.900510e-06;
       hostDelays[0][station +  1 * 48][0] = 5.496243e-06;
       hostDelays[0][station +  1 * 48][1] = 5.495999e-06;
       hostDelays[0][station +  2 * 48][0] = 6.470178e-06;
       hostDelays[0][station +  2 * 48][1] = 6.470559e-06;
       hostDelays[0][station +  3 * 48][0] = 7.119765e-06;
       hostDelays[0][station +  3 * 48][1] = 7.120457e-06;
       hostDelays[0][station +  4 * 48][0] = 6.466346e-06;
       hostDelays[0][station +  4 * 48][1] = 6.467317e-06;
       hostDelays[0][station +  5 * 48][0] = 6.501126e-06;
       hostDelays[0][station +  5 * 48][1] = 6.501473e-06;
       hostDelays[0][station +  6 * 48][0] = 4.674506e-06;
       hostDelays[0][station +  6 * 48][1] = 4.674821e-06;
       hostDelays[0][station +  7 * 48][0] = 7.486790e-06;
       hostDelays[0][station +  7 * 48][1] = 7.487222e-06;
       hostDelays[0][station +  8 * 48][0] = 8.710292e-06;
       hostDelays[0][station +  8 * 48][1] = 8.710297e-06;
       hostDelays[0][station +  9 * 48][0] = 1.533884e-05;
       hostDelays[0][station +  9 * 48][1] = 1.533871e-05;
       hostDelays[0][station +  10 * 48][0] = 5.977083e-06;
       hostDelays[0][station +  10 * 48][1] = 5.976727e-06;
       hostDelays[0][station +  11 * 48][0] = 8.474628e-06;
       hostDelays[0][station +  11 * 48][1] = 8.475407e-06;
      }
    } else if (ps.nrStations() == 1152) {
      for (unsigned station = 0; station < 48; station ++) {
       hostDelays[0][station +  0 * 48][0] = 6.899769e-06;
       hostDelays[0][station +  0 * 48][1] = 6.900510e-06;
       hostDelays[0][station +  1 * 48][0] = 5.496243e-06;
       hostDelays[0][station +  1 * 48][1] = 5.495999e-06;
       hostDelays[0][station +  2 * 48][0] = 6.470178e-06;
       hostDelays[0][station +  2 * 48][1] = 6.470559e-06;
       hostDelays[0][station +  3 * 48][0] = 7.119765e-06;
       hostDelays[0][station +  3 * 48][1] = 7.120457e-06;
       hostDelays[0][station +  4 * 48][0] = 6.466346e-06;
       hostDelays[0][station +  4 * 48][1] = 6.467317e-06;
       hostDelays[0][station +  5 * 48][0] = 6.501126e-06;
       hostDelays[0][station +  5 * 48][1] = 6.501473e-06;
       hostDelays[0][station +  6 * 48][0] = 4.674506e-06;
       hostDelays[0][station +  6 * 48][1] = 4.674821e-06;
       hostDelays[0][station +  7 * 48][0] = 7.486790e-06;
       hostDelays[0][station +  7 * 48][1] = 7.487222e-06;
       hostDelays[0][station +  8 * 48][0] = 8.710292e-06;
       hostDelays[0][station +  8 * 48][1] = 8.710297e-06;
       hostDelays[0][station +  9 * 48][0] = 1.533884e-05;
       hostDelays[0][station +  9 * 48][1] = 1.533871e-05;
       hostDelays[0][station +  10 * 48][0] = 5.977083e-06;
       hostDelays[0][station +  10 * 48][1] = 5.976727e-06;
       hostDelays[0][station +  11 * 48][0] = 8.474628e-06;
       hostDelays[0][station +  11 * 48][1] = 8.475407e-06;
       hostDelays[0][station +  12 * 48][0] = 4.580886e-06;
       hostDelays[0][station +  12 * 48][1] = 4.580770e-06;
       hostDelays[0][station +  13 * 48][0] = 1.613452e-05;
       hostDelays[0][station +  13 * 48][1] = 1.613434e-05;
       hostDelays[0][station +  14 * 48][0] = 1.688270e-05;
       hostDelays[0][station +  14 * 48][1] = 1.688266e-05;
       hostDelays[0][station +  15 * 48][0] = 9.642859e-06;
       hostDelays[0][station +  15 * 48][1] = 9.643464e-06;
       hostDelays[0][station +  16 * 48][0] = 6.294257e-06;
       hostDelays[0][station +  16 * 48][1] = 6.294251e-06;
       hostDelays[0][station +  17 * 48][0] = 1.507046e-05;
       hostDelays[0][station +  17 * 48][1] = 1.507095e-05;
       hostDelays[0][station +  18 * 48][0] = 3.541655e-05;
       hostDelays[0][station +  18 * 48][1] = 3.541721e-05;
       hostDelays[0][station +  19 * 48][0] = 1.738412e-05;
       hostDelays[0][station +  19 * 48][1] = 1.738473e-05;
       hostDelays[0][station +  20 * 48][0] = 7.617290e-06;
       hostDelays[0][station +  20 * 48][1] = 7.619594e-06;
       hostDelays[0][station +  21 * 48][0] = 1.225280e-05;
       hostDelays[0][station +  21 * 48][1] = 1.225299e-05;
       hostDelays[0][station +  22 * 48][0] = 7.852150e-06;
       hostDelays[0][station +  22 * 48][1] = 7.852323e-06;
       hostDelays[0][station +  23 * 48][0] = 1.651116e-05;
       hostDelays[0][station +  23 * 48][1] = 1.651105e-05;
      }
    } else {
#pragma omp once
#pragma omp critical (cerr)
      std::cerr << "WARNING: I do not know which delays to apply" << std::endl;
    }
  }
}


bool CorrelatorWorkQueue::hasValidData(std::vector<SparseSet<TimeStamp> > &validData)
{
  for (SparseSet<TimeStamp> &set : validData)
    if (!set.empty())
      return true;

  return false;
}


void CorrelatorWorkQueue::doWork()
{
#if !defined CL_DEVICE_TOPOLOGY_AMD
#pragma omp for schedule(dynamic), collapse(2), ordered // nowait
  for (TimeStamp time = ps.startTime(); time < ps.stopTime(); time += ps.nrSamplesPerSubbandBeforeFilter())
    for (unsigned subband = 0; subband < ps.nrSubbands(); subband ++)
      doSubband(time, subband);
#else
  TimeStamp time;
  unsigned  subband;

  while (pipeline.getWork(deviceInstance.numaNode, time, subband))
    doSubband(time, subband);
#endif
}


bool CorrelatorWorkQueue::inTime(const TimeStamp &time)
{
  return !ps.realTime() || (int64_t) TimeStamp::now(ps.clockSpeed()) - (int64_t) time < ps.nrRingBufferSamplesPerSubband() - ps.nrSamplesPerSubbandBeforeFilter();
}


void CorrelatorWorkQueue::computeWeights(const std::vector<SparseSet<TimeStamp> > &validData, Visibilities *visibilities)
{
  for (unsigned stat2 = 0, pair = 0; stat2 < validData.size(); stat2 ++)
    for (unsigned stat1 = 0; stat1 <= stat2 && pair < sizeof(visibilities->header.weights) / sizeof(visibilities->header.weights[0]); stat1 ++, pair ++)
      visibilities->header.weights[pair] = ((uint32_t) (int64_t) (validData[stat1] & validData[stat2]).count() / ps.nrChannelsPerSubband() - (NR_TAPS - 1)) * ps.channelIntegrationFactor();
}


void CorrelatorWorkQueue::doSubband(const TimeStamp &time, unsigned subband)
{
  pipeline.startReadTransaction(time);
  pipeline.inputSection.fillInMissingSamples(time, subband, validData);

  if (hasValidData(validData) && inTime(time)) {
    std::unique_ptr<Visibilities> visibilities = pipeline.outputSection.getVisibilitiesBuffer(subband);
    std::function<void (cu::Stream &, cu::DeviceMemory &, PerformanceCounter &)> enqueueCopyInputBuffer = [=] (cu::Stream &stream, cu::DeviceMemory &devInputBuffer, PerformanceCounter &counter)
    {
      pipeline.inputSection.enqueueHostToDeviceCopy(stream, devInputBuffer, counter, time, subband);
    };

    unsigned nrHistorySamples = (NR_TAPS - 1) * ps.nrChannelsPerSubband();
    unsigned startIndex = (time - nrHistorySamples) % ps.nrRingBufferSamplesPerSubband();
    deviceInstance.doSubband(time, subband, enqueueCopyInputBuffer, pipeline.inputSection.hostRingBuffers[subband], hostDelays, hostDelays, visibilities->hostVisibilities, startIndex);

    visibilities->startTime = time;
    visibilities->endTime = time + ps.nrSamplesPerSubbandAfterFilter();
    computeWeights(validData, visibilities.get());
    pipeline.outputSection.putVisibilitiesBuffer(std::move(visibilities), time, subband);
  } else {
    if (subband == 0)
#pragma omp critical (clog)
      std::clog << "Warning: no valid samples for block starting at " << time << std::endl;

    pipeline.outputSection.putVisibilitiesBuffer(nullptr, time, subband);
  }

  pipeline.endReadTransaction(time);
}
