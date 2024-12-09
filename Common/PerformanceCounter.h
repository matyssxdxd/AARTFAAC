#ifndef PERFORMANCE_COUNTER_H
#define PERFORMANCE_COUNTER_H

#include "Common/PowerSensor.h"

#include <cudawrappers/cu.hpp>

#if defined MEASURE_POWER
#include <PowerSensor.hpp>
#endif


class PerformanceCounter
{
  public:
    class Measurement {
      public:
	Measurement(PerformanceCounter &, cu::Stream &, size_t nrOperations, size_t nrBytesRead, size_t nrBytesWritten);
	~Measurement();

      private:
	struct State {
	  State(PerformanceCounter &counter) : counter(counter) {}

	  cu::Event          startEvent, stopEvent;
	  PerformanceCounter &counter;

#if defined MEASURE_POWER
	  PowerSensor3::State psStartState;
#endif
	} *state;

	PerformanceCounter &counter;
	cu::Stream         &stream;
    };

    PerformanceCounter(const std::string &name, bool profiling);
    ~PerformanceCounter() noexcept(false);

  private:
    friend class Measurement;

  public:
    size_t	      totalNrOperations, totalNrBytesRead, totalNrBytesWritten;
#if defined MEASURE_POWER
    double	      totalJoules;
#endif
    double	      totalTime;
    unsigned	      nrTimes;
    const std::string name;
    unsigned	      counterNumber;
    static unsigned   nrCounters;
    bool	      profiling;
};

#endif
