#if !defined TRANSPOSE_KERNEL_H
#define TRANSPOSE_KERNEL_H

#include "Common/Function.h"
#include "Common/PerformanceCounter.h"
#include "Correlator/Parset.h"


class TransposeKernel : public Function
{
  public:
    TransposeKernel(const CorrelatorParset &, const Module &);

    void launchAsync(cu::Stream &,
		     cu::DeviceMemory &output,
		     const cu::DeviceMemory &input,
		     unsigned startIndex,
		     PerformanceCounter &);

  private:
    const bool hasIntegratedMemory;
};

#endif
