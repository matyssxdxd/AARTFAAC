#include "Common/Config.h"

#include "Common/Align.h"
#include "Correlator/Kernels/DelayAndBandPassKernel.h"

#include <cassert>
#include <complex>


DelayAndBandPassKernel::DelayAndBandPassKernel(const Parset &ps, cl::Program &program, cl::Buffer &devCorrectedData, cl::Buffer &devFilteredData, cl::Buffer &devDelaysAtBegin, cl::Buffer &devDelaysAfterEnd, /* cl::Buffer &devPhaseOffsets, */ cl::Buffer &devBandPassCorrectionWeights)
:
  Kernel(program, "applyDelaysAndCorrectBandPass"),
  ps(ps)
{
  //assert(ps.nrChannelsPerSubband() % 16 == 0 || ps.nrChannelsPerSubband() == 1);
  assert(ps.nrSamplesPerChannelAfterFilter() % 16 == 0);

  setArg(0, devCorrectedData);
  setArg(1, devFilteredData);
  setArg(4, devDelaysAtBegin);
  setArg(5, devDelaysAfterEnd);
  //setArg(6, devPhaseOffsets);
  setArg(6 /* 7 */, devBandPassCorrectionWeights);

  globalWorkSize = cl::NDRange(align(ps.nrStations() * ps.nrPolarizations(), 16), ps.nrChannelsPerSubband());
  localWorkSize = cl::NDRange(16, 16);

  size_t nrSamples = ps.nrStations() * ps.nrChannelsPerSubband() * ps.nrSamplesPerChannelAfterFilter() * ps.nrPolarizations();
  nrOperations = nrSamples * (ps.delayCompensation() ? 12 : ps.correctBandPass() ? 2 : 0);
  nrBytesRead = nrBytesWritten = nrSamples * sizeof(std::complex<float>);
}


void DelayAndBandPassKernel::enqueue(cl::CommandQueue &queue, PerformanceCounter &counter, unsigned subband)
{
  setArg(2, (float) (ps.delayCompensation() ? ps.subbandFrequencies()[subband] : 0));
  setArg(3, 0);       // beam
  Kernel::enqueue(queue, counter);
}
