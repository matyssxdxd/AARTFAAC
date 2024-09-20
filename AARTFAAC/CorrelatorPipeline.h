#ifndef AARTFAAC_CORRELATOR_PIPELINE
#define AARTFAAC_CORRELATOR_PIPELINE

#include "AARTFAAC/InputSection.h"
#include "AARTFAAC/OutputSection.h"
#include "Correlator/CorrelatorPipeline.h"
#include "Common/FilterBank.h"
#include "Common/PerformanceCounter.h"
#include "Common/SlidingPointer.h"

#include <bitset>
#include <csignal>
#include <string>
#include <mutex>
#include <vector>


class AARTFAAC_CorrelatorPipeline : public CorrelatorPipeline
{
  public:
			   AARTFAAC_CorrelatorPipeline(const AARTFAAC_Parset &);

    bool		   getWork(unsigned preferredNode, TimeStamp &, unsigned &subband);
    void		   doWork();

    void		   startReadTransaction(const TimeStamp &);
    void		   endReadTransaction(const TimeStamp &);

    static void		   caughtSignal();

    const AARTFAAC_Parset  &ps;
    const unsigned	   nrWorkQueues;
    InputSection	   inputSection;
    OutputSection	   outputSection;

    std::vector<TimeStamp> currentTimes;
    std::mutex		   currentTimesMutex;
    SlidingPointer<TimeStamp> currentTime;

  private:
    void		   logProgress(const TimeStamp &time) const;

    std::mutex		   getWorkLock;
    std::bitset<64>	   subbandsDone;
    TimeStamp		   nextTime;

    static volatile std::sig_atomic_t signalCaught;
};

#endif
