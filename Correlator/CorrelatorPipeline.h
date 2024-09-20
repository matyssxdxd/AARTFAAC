#ifndef CORRELATOR_PIPELINE
#define CORRELATOR_PIPELINE

#include "Common/PerformanceCounter.h"
#include "Common/FilterBank.h"
#include "Correlator/DeviceInstance.h"
#include "Correlator/Parset.h"

#if defined MEASURE_POWER
#include "PowerSensor.h"
#endif

#include <string>


class CorrelatorPipeline
{
  public:
			CorrelatorPipeline(const CorrelatorParset &);

    const CorrelatorParset &ps;
    FilterBank		filterBank;

    std::vector<std::unique_ptr<DeviceInstance>> deviceInstances;

    PerformanceCounter	transposeCounter, filterAndCorrectCounter, postTransposeCounter, correlateCounter;
    PerformanceCounter	samplesCounter, visibilitiesCounter;

#if defined MEASURE_POWER
    PowerSensor::State	startState, stopState;
#endif

  private:
    void createDeviceInstances();
};


#endif
