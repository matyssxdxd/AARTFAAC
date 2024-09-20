#ifndef PERFORMANCE_COUNTER_H
#define PERFORMANCE_COUNTER_H

#include "Common/PowerSensor.h"

#include </var/scratch/jsteinbe/tensor-core-correlator/build/_deps/cudawrappers-src/include/cudawrappers/cu.hpp>

#if defined MEASURE_POWER
#include <string>
#endif


class PerformanceCounter
{
  public:
    class Measurement {
      public:
	Measurement(PerformanceCounter &, cu::Stream &, size_t nrOperations, size_t nrBytesRead, size_t nrBytesWritten);
	~Measurement();

      private:
	PerformanceCounter &counter;
	cu::Stream         &stream;
	cu::Event          startEvent;

#if defined MEASURE_POWER
	PowerSensor::State psState;
#endif
    };

    PerformanceCounter(const std::string &name, bool profiling);
    ~PerformanceCounter() noexcept(false);

  private:
    friend class Measurement;

#if defined MEASURE_POWER
    static void       startPowerMeasurement(cl_event, cl_int /*status*/, void *descriptor);
    static void       stopPowerMeasurement(cl_event, cl_int /*status*/, void *descriptor);

    double	      totalJoules;
#endif

  public:
    size_t	      totalNrOperations, totalNrBytesRead, totalNrBytesWritten;
    double	      totalTime;
    unsigned	      nrTimes;
    const std::string name;
    unsigned	      counterNumber;
    static unsigned   nrCounters;
    bool	      profiling;
};

#endif
