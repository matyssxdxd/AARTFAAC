#if !defined TCC_H
#define TCC_H

#include "Common/PerformanceCounter.h"
#include "Correlator/Parset.h"

#include <cudawrappers/cu.hpp>

#include <libtcc/Correlator.h>


class TCC
{
  public:
    TCC(const cu::Device &, const CorrelatorParset &);

    void launchAsync(cu::Stream &, cu::DeviceMemory &devVisiblities, const cu::DeviceMemory &devSamples, PerformanceCounter &);

  private:
    tcc::Correlator tcc;
};


#endif
