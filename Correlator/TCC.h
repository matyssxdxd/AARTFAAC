#if !defined TCC_H
#define TCC_H

#include "Common/PerformanceCounter.h"
#include "Correlator/Parset.h"

#include </var/scratch/jsteinbe/tensor-core-correlator/build/_deps/cudawrappers-src/include/cudawrappers/cu.hpp>

#include </var/scratch/jsteinbe/install/tcc/include/libtcc/Correlator.h>


class TCC
{
  public:
    TCC(const CorrelatorParset &);

    void launchAsync(cu::Stream &, cu::DeviceMemory &devVisiblities, const cu::DeviceMemory &devSamples, PerformanceCounter &);

  private:
    tcc::Correlator tcc;
};


#endif
