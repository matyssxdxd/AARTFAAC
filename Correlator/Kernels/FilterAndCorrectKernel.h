#if !defined FILTER_AND_CORRECT_KERNEL_H
#define FILTER_AND_CORRECT_KERNEL_H

#include "Common/Function.h"
#include "Common/PerformanceCounter.h"
#include "Correlator/Parset.h"


class FilterAndCorrectKernel : public Function
{
  public:
    FilterAndCorrectKernel(const CorrelatorParset &, const Module &);

    void launchAsync(cu::Stream &,
		     cu::DeviceMemory &devCorrectedData,
		     const cu::DeviceMemory &devSamples,
		     const cu::DeviceMemory &devFIRfilterWeights,
		     const cu::DeviceMemory &devDelays,
		     const cu::DeviceMemory &devBandPassWeights,
		     //double subbandFrequency,
		     PerformanceCounter &);

  private:
    size_t nrOperations;
};

#endif
