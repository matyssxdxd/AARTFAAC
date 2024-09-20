#if !defined POST_TRANSPOSE_KERNEL_H
#define POST_TRANSPOSE_KERNEL_H

#include "Common/Function.h"
#include "Common/PerformanceCounter.h"
#include "Correlator/Parset.h"


class PostTransposeKernel : public Function
{
  public:
    PostTransposeKernel(const CorrelatorParset &, const Module &);

    void launchAsync(cu::Stream &,
		     cu::DeviceMemory &output,
		     const cu::DeviceMemory &input,
		     PerformanceCounter &);
};

#endif
