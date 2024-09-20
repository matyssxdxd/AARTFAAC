#include "Common/Config.h"

#include "Common/PerformanceCounter.h"

#include <functional>
#include <iomanip>
#include <iostream>


unsigned PerformanceCounter::nrCounters;


PerformanceCounter::PerformanceCounter(const std::string &name, bool profiling)
:
#if defined MEASURE_POWER
  totalJoules(0),
#endif
  totalNrOperations(0),
  totalNrBytesRead(0),
  totalNrBytesWritten(0),
  totalTime(0),
  nrTimes(0),
  name(name),
  profiling(profiling)
{
//#pragma omp atomic
  counterNumber = nrCounters ++;
}


PerformanceCounter::~PerformanceCounter() noexcept(false)
{
  if (totalTime > 0)
#pragma omp critical (cout)
  {
    std::cout << std::setw(12) << name
	      << std::setprecision(3) << ": "
	      << nrTimes << " times, "
	      << totalTime / nrTimes * 1e3 << " ms";

    if (totalNrOperations != 0)
      std::cout << ", " << totalNrOperations / totalTime * 1e-12 << " TFLOPS";

    if (totalNrBytesRead != 0 || totalNrBytesWritten != 0)
      std::cout << ", " << totalNrBytesRead / totalTime * 1e-9 << '+'
		<< totalNrBytesWritten / totalTime * 1e-9 << '='
		<< (totalNrBytesRead + totalNrBytesWritten) / totalTime * 1e-9 << " GB/s (R+W)";

#if defined MEASURE_POWER
    if (totalNrOperations > 0)
      std::cout << ", " << totalJoules / totalTime << " W"
		", " << totalNrOperations / totalJoules * 1e-9 << " GFLOPS/W";
#endif

    std::cout << std::endl;
  }
}


#if defined MEASURE_POWER

struct PowerDescriptor
{
  PowerSensor::State startState, stopState;
  PerformanceCounter *counter;
};


void PerformanceCounter::startPowerMeasurement(cl_event ev, cl_int /*status*/, void *arg)
{
  static_cast<PowerDescriptor *>(arg)->startState = powerSensor.read();
}


void PerformanceCounter::stopPowerMeasurement(cl_event ev, cl_int /*status*/, void *arg)
{
  PowerDescriptor *descriptor = static_cast<PowerDescriptor *>(arg);
  descriptor->stopState = powerSensor.read();

#pragma omp atomic
  descriptor->counter->totalJoules += PowerSensor::Joules(descriptor->startState, descriptor->stopState);

  const char *name = descriptor->counter->name.c_str();
  powerSensor.mark(descriptor->startState, /* descriptor->stopState, */ name, descriptor->counter->counterNumber);

  delete descriptor;
}

#endif


PerformanceCounter::Measurement::Measurement(PerformanceCounter &counter, cu::Stream &stream, size_t nrOperations, size_t nrBytesRead, size_t nrBytesWritten)
:
  counter(counter),
  stream(stream)
{
#if 1
  if (counter.profiling) {
    stream.record(startEvent);

#if defined MEASURE_POWER
#error FIXME
    PowerDescriptor *descriptor = new PowerDescriptor;
    descriptor->counter = this;

    // AMD: kernel execution starts at "CL_SUBMITTED" and stops at "CL_RUNNING"
    event.setCallback(CL_SUBMITTED, &PerformanceCounter::startPowerMeasurement, descriptor);
    event.setCallback(CL_RUNNING, &PerformanceCounter::stopPowerMeasurement, descriptor);
#endif

#pragma omp atomic
    counter.totalNrOperations   += nrOperations;
#pragma omp atomic
    counter.totalNrBytesRead    += nrBytesRead;
#pragma omp atomic
    counter.totalNrBytesWritten += nrBytesWritten;
#pragma omp atomic
    ++ counter.nrTimes;
  }
#endif
}


PerformanceCounter::Measurement::~Measurement()
{
  if (counter.profiling) {
    cu::Event stopEvent, &startEvent = this->startEvent;
    double &totalTime = counter.totalTime;

    stream.record(stopEvent);
    stream.addCallback([] (CUstream, CUresult, void *arg) { // stateless capture (converts to C-style function pointer)
      std::function<void ()> *closure = static_cast<std::function<void ()> *>(arg);
      (*closure)();
      delete closure;
    }, new std::function<void ()> ([&, startEvent, stopEvent] () { // statefull capture
#pragma omp atomic
      //counter.totalTime += 1e-3 * stopEvent.elapsedTime(startEvent);
      totalTime += 1e-3 * stopEvent.elapsedTime(startEvent);
    }));
  }
}
