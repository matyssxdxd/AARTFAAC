#ifndef UNIT_TEST
#define UNIT_TEST

#include "Common/OpenCL_Support.h"
#include "Common/PerformanceCounter.h"
#include "Common/Parset.h"
#include "Common/Program.h"

#include <iostream>


class UnitTest
{
  protected:
    UnitTest(const Parset &ps, const char *programName = 0, const std::string &extraArgs = "")
    :
      counter(programName != 0 ? programName : "test", ps.profiling())
    {
      createContext(context, devices);
      queue = cl::CommandQueue(context, devices[0], CL_QUEUE_PROFILING_ENABLE);

      if (programName != 0)
	    program = createProgram(ps, context, devices, programName, extraArgs);
    }

    template <typename T> void check(T actual, T expected)
    {
      if (expected != actual)
      {
	      std::cerr << "Test FAILED: expected " << expected << ", computed " << actual << std::endl;
	      exit(1);
      }
      else
      {
	      std::cout << "Test OK" << std::endl;
      }
    }

    template <typename T> void checkNonZero(T actual)
    {
      if (actual == (T)0)
      {
	      std::cerr << "Test FAILED: expected nonzero value, computed " << actual << std::endl;
	      exit(1);
      }
      else
      {
	      std::cout << "Test OK" << std::endl;
      }
    }

    cl::Context context;
    std::vector<cl::Device> devices;
    cl::Program program;
    cl::CommandQueue queue;

    PerformanceCounter counter;
};

#endif
