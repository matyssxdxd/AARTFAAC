#include "Common/Config.h"
#include "Common/CUDA_Support.h"
#include "Correlator/CorrelatorPipeline.h"
#include "Correlator/DeviceInstance.h"

#include <omp.h>

#include <iostream>
#include <memory>


CorrelatorPipeline::CorrelatorPipeline(const CorrelatorParset &ps)
:
  ps(ps),
  filterBank(true, NR_TAPS, ps.nrChannelsPerSubband(), KAISER),
  deviceInstances(ps.nrGPUs()),
  transposeCounter("transpose", ps.profiling()),
  filterAndCorrectCounter("filt.correct", ps.profiling()),
  postTransposeCounter("postTransp.", ps.profiling()),
  correlateCounter("correlate", ps.profiling()),
  samplesCounter("samples", ps.profiling()),
  visibilitiesCounter("visibilities", ps.profiling())
{
  filterBank.negateWeights();
  //filterBank.printWeights();
  createDeviceInstances();
}


void CorrelatorPipeline::createDeviceInstances()
{
  std::mutex	     mutex;
  std::exception_ptr exception_ptr;

#pragma omp parallel for schedule(dynamic)
  for (unsigned deviceIndex = 0; deviceIndex < ps.nrGPUs(); deviceIndex ++)
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
    try {
#endif
      if (ps.GPUs()[deviceIndex] >= cu::Device::getCount()) {
	std::stringstream str;
	str << "GPU " << (signed) ps.GPUs()[deviceIndex] << " does not exit";
	throw Parset::Error(str.str());
      } else {
        deviceInstances[deviceIndex] = std::unique_ptr<DeviceInstance>(cu::Device(deviceIndex).getAttribute(CU_DEVICE_ATTRIBUTE_INTEGRATED) ? new DeviceInstance(*this, deviceIndex) : new DeviceInstanceWithoutUnifiedMemory(*this, deviceIndex));
      }
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
    } catch (...) {
      std::lock_guard<std::mutex> lock(mutex);
      exception_ptr = std::current_exception();
    }
#endif

  if (exception_ptr != nullptr)
    std::rethrow_exception(exception_ptr);
}
