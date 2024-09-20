#include "Common/Config.h"
#include "Correlator/Kernels/PostTransposeKernel.h"


PostTransposeKernel::PostTransposeKernel(const CorrelatorParset &ps, const Module &module)
:
  Function(ps, module, "postTranspose")
{
}


void PostTransposeKernel::launchAsync(cu::Stream &stream,
				      cu::DeviceMemory &devOutput,
				      const cu::DeviceMemory &devInput,
				      PerformanceCounter &counter)
{
  PerformanceCounter::Measurement measurement(counter, stream, 0, 0, 0);

  std::vector<const void *> parameters = {
    devOutput.parameter(),
    devInput.parameter(),
  };

  unsigned nrTimesPerBlock = 128 / ps.nrBitsPerSample();
  unsigned nrRecvPolPerBlock = nrTimesPerBlock < 64 ? 64 / nrTimesPerBlock : 1;

  stream.launchKernel(*this,
		      (ps.nrStations() * ps.nrPolarizations() + nrRecvPolPerBlock - 1) / nrRecvPolPerBlock, (ps.nrSamplesPerChannel() + nrTimesPerBlock - 1) / nrTimesPerBlock, (ps.nrChannelsPerSubband() + 63) / 64,
                      32, 32, 1,
                      0, parameters);
}
