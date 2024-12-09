#if !defined FILTER_KERNEL_H
#define FILTER_KERNEL_H
#include "libfilter/Kernel.h"
namespace tcc {
  class FilterKernel : public Kernel
  {
    public:
      FilterKernel(cu::Module &module,
		   unsigned nrBits,
		   unsigned nrReceivers,
		   unsigned nrTaps,
		   unsigned nrChannels,
		   unsigned nrSamplesPerChannel,
		   unsigned nrPolarizations = 2
		  );
      //void launchAsync(cu::Stream &, cu::DeviceMemory &deviceVisibilities, cu::DeviceMemory &deviceSamples, bool add = false);
      virtual uint64_t FLOPS() const;
    private:
      const unsigned nrBits;
      const unsigned nrReceivers;
      const unsigned nrTaps;
      const unsigned nrChannels;
      const unsigned nrSamplesPerChannel;
      const unsigned nrPolarizations;
      //unsigned       nrThreadBlocksPerChannel;
  };
}
#endif
