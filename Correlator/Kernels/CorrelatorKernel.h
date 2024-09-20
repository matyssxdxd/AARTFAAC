#ifndef _CORRELATOR_KERNEL_H
#define _CORRELATOR_KERNEL_H

#include "Common/Kernel.h"
#include "Correlator/Parset.h"

class CorrelateSquareKernel : public Kernel
{
  public:
    CorrelateSquareKernel(const CorrelatorParset &, cl::CommandQueue &, cl::Program &, cl::Buffer &devVisibilities, cl::Buffer &devCorrectedData);
};

class CorrelateRectangleKernel : public Kernel
{
  public:
    CorrelateRectangleKernel(const CorrelatorParset &, cl::CommandQueue &, cl::Program &, cl::Buffer &devVisibilities, cl::Buffer &devCorrectedData);
};

class CorrelateTriangleKernel : public Kernel
{
  public:
    CorrelateTriangleKernel(const CorrelatorParset &, cl::CommandQueue &, cl::Program &, cl::Buffer &devVisibilities, cl::Buffer &devCorrectedData);
};

#endif
