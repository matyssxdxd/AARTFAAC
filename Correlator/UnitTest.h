#if !defined CORRELATOR_UNIT_TEST
#define CORRELATOR_UNIT_TEST

#include "Common/UnitTest.h"
#include "Correlator/Parset.h"

#include <sstream>


class CorrelatorUnitTest : public UnitTest
{
  public:
    CorrelatorUnitTest(const CorrelatorParset &ps, const char *programName)
    :
      UnitTest(ps, programName, [&ps] {
	      std::stringstream args;
	      args << " -DCORRELATION_MODE=" << ps.correlationMode();
	      args << " -DNR_OUTPUT_CHANNELS=" << ps.nrOutputChannelsPerSubband();
	      return args.str();
      } ())
    {
    }
};

#endif
