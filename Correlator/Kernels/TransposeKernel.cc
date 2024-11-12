#include "Common/Config.h"
#include "Correlator/Kernels/TransposeKernel.h"


TransposeKernel::TransposeKernel(const CorrelatorParset &ps, const cu::Device &device, const Module &module)
:
  Function(ps, module, "transpose"),
  hasIntegratedMemory(device.getAttribute<CU_DEVICE_ATTRIBUTE_INTEGRATED>())
{
}


void TransposeKernel::launchAsync(cu::Stream &stream,
				  cu::DeviceMemory &devOutput,
				  const cu::DeviceMemory &devInput,
				  unsigned startIndex,
				  PerformanceCounter &counter)
{
  PerformanceCounter::Measurement measurement(counter, stream, 0, 0, 0);

  std::vector<const void *> parameters = {
    devOutput.parameter(),
    devInput.parameter(),
  };

  if (hasIntegratedMemory)
    parameters.push_back(&startIndex);

  stream.launchKernel(*this,
		      (ps.nrStations() * ps.nrPolarizations() + 63) / 64, ((ps.nrSamplesPerChannel() + NR_TAPS - 1) * ps.nrChannelsPerSubband() + 63) / 64, 1,
                      32, 32, 1,
                      0, parameters);
}
