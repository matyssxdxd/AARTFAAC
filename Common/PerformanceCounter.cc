#include "Common/Config.h"

#include "Common/PerformanceCounter.h"

#include <functional>
#include <iomanip>
#include <iostream>


unsigned PerformanceCounter::nrCounters;


PerformanceCounter::PerformanceCounter(const std::string &name, bool profiling)
:
  totalNrOperations(0),
  totalNrBytesRead(0),
  totalNrBytesWritten(0),
#if defined MEASURE_POWER
  totalJoules(0),
#endif
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


PerformanceCounter::Measurement::Measurement(PerformanceCounter &counter, cu::Stream &stream, size_t nrOperations, size_t nrBytesRead, size_t nrBytesWritten)
:
  state(new PerformanceCounter::Measurement::State(counter)),
  counter(counter),
  stream(stream)
{
  if (counter.profiling) {
    stream.record(state->startEvent);

#if defined MEASURE_POWER
    stream.addCallback([] (CUstream, CUresult, void *arg) {
      static_cast<State *>(arg)->psStartState = powerSensor.read();
    }, state);
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
}


PerformanceCounter::Measurement::~Measurement()
{
  if (counter.profiling) {
    stream.record(state->stopEvent);

    stream.addCallback([] (CUstream, CUresult, void *arg) {
      State *state = static_cast<State *>(arg);

#pragma omp atomic
      state->counter.totalTime += 1e-3 * state->stopEvent.elapsedTime(state->startEvent);

#if defined MEASURE_POWER
      PowerSensor3::State psStopState = powerSensor.read();

#pragma omp atomic
      state->counter.totalJoules += PowerSensor3::Joules(state->psStartState, psStopState);
#endif

      delete state;
    }, state);

#if 0
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
#endif
  }
}
