#include "Common/Config.h"
#include "Correlator/Kernels/FilterAndCorrectKernel.h"


FilterAndCorrectKernel::FilterAndCorrectKernel(const CorrelatorParset &ps, const Module &module)
:
  Function(ps, module, "filterAndCorrect"),
  nrOperations((size_t) ps.nrStations() * ps.nrPolarizations() * ps.nrSamplesPerSubband() * (4 * NR_TAPS + 5 * std::log(ps.nrChannelsPerSubband()) + 2 + 8))
{
}


void FilterAndCorrectKernel::launchAsync(cu::Stream &stream,
					 cu::DeviceMemory &devCorrectedData,
					 const cu::DeviceMemory &devSamples,
					 const cu::DeviceMemory &devFIRfilterWeights,
					 const cu::DeviceMemory &devDelays,
					 const cu::DeviceMemory &devBandPassWeights,
					 //double subbandFrequency,
					 PerformanceCounter &counter)
{
  PerformanceCounter::Measurement measurement(counter, stream, nrOperations, 0, 0);

  std::vector<const void *> parameters = {
    devCorrectedData.parameter(),
    devSamples.parameter(),
    devFIRfilterWeights.parameter(),
    devDelays.parameter(),
    devBandPassWeights.parameter(),
    //devBandPassWeights.parameter(),
    // FIXME &subbandFrequency
  };

#if 1
  stream.launchKernel(*this,
                      ps.nrPolarizations(), ps.nrStations(), 1,
                      ps.nrChannelsPerSubband() / 16, 16, 1,
                      0, parameters);
#else
  unsigned maxBlocksPerMultiProcessor = occupancyMaxActiveBlocksPerMultiprocessor(ps.nrChannelsPerSubband(), 0);
  unsigned nrMultiProcessors          = stream.getContext().getDevice().getAttribute(CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT);

  std::cout << "max = " << maxBlocksPerMultiProcessor << " * " << nrMultiProcessors << std::endl;
  stream.launchCooperativeKernel(*this,
				 maxBlocksPerMultiProcessor * nrMultiProcessors, 1, 1,
				 //ps.nrChannelsPerSubband() / 16, 16, 1,
				 ps.nrChannelsPerSubband(), 1, 1,
				 0, parameters);
#endif

}
