#ifndef DELAY_AND_BAND_PASS_KERNEL_H
#define DELAY_AND_BAND_PASS_KERNEL_H

#include "Common/Kernel.h"
#include "Correlator/Parset.h"


class DelayAndBandPassKernel : public Kernel
{
  public:
    DelayAndBandPassKernel(const Parset &, cl::Program &, cl::Buffer &devCorrectedData, cl::Buffer &devFilteredData, cl::Buffer &devDelaysAtBegin, cl::Buffer &devDelaysAfterEnd, /* cl::Buffer &devPhaseOffsets, */ cl::Buffer &devBandPassCorrectionWeights);

    void enqueue(cl::CommandQueue &, PerformanceCounter &, unsigned subband);

  private:
    const Parset &ps;
};

#endif

