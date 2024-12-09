#include "Correlator/Kernels/FilterKernel.h"


namespace tcc {

FilterKernel::FilterKernel(cu::Module &module,
			   unsigned nrBits,
			   unsigned nrReceivers,
			   unsigned nrTaps,
			   unsigned nrChannels,
			   unsigned nrSamplesPerChannel,
			   unsigned nrPolarizations
			  )
:
  Kernel(module, "filterAndCorrect"),
  nrBits(nrBits),
  nrReceivers(nrReceivers),
  nrTaps(nrTaps),
  nrChannels(nrChannels),
  nrSamplesPerChannel(nrSamplesPerChannel),
  nrPolarizations(nrPolarizations)
{
  //unsigned blocksPerDim = (nrReceivers + nrReceiversPerBlock - 1) / nrReceiversPerBlock;
  //nrThreadBlocksPerChannel = nrReceiversPerBlock == 64 ? blocksPerDim * blocksPerDim : blocksPerDim * (blocksPerDim + 1) / 2;
}


#if 0
void FilterKernel::launchAsync(cu::Stream &stream, cu::DeviceMemory &deviceVisibilities, cu::DeviceMemory &deviceSamples, bool add)
{
  std::vector<const void *> parameters = {
    deviceVisibilities.parameter(),
    deviceSamples.parameter(),
    &add
  };

  stream.launchKernel(function,
		      nrThreadBlocksPerChannel, nrChannels, 1,
		      32, 2, 2,
		      0, parameters);
}
#endif


uint64_t FilterKernel::FLOPS() const
{
  return 2ULL * nrReceivers * nrTaps * nrPolarizations * nrChannels * nrSamplesPerChannel;
}

}


