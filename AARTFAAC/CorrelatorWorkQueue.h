#ifndef CORRELATOR_WORK_QUEUE
#define CORRELATOR_WORK_QUEUE

#include "AARTFAAC/CorrelatorPipeline.h"
#include "Common/CUDA_Support.h"
#include "Common/SparseSet.h"
#include "Common/TimeStamp.h"
#include "Correlator/DeviceInstance.h"

#include <vector>


class CorrelatorWorkQueue
{
  public:
    CorrelatorWorkQueue(AARTFAAC_CorrelatorPipeline &, DeviceInstance &);

    void doWork();
    void doSubband(const TimeStamp &, unsigned subband);

    const AARTFAAC_Parset	   &ps;
    AARTFAAC_CorrelatorPipeline    &pipeline;
    DeviceInstance		   &deviceInstance;

    MultiArrayHostBuffer<float, 3> hostDelays;

  private:
    bool hasValidData(std::vector<SparseSet<TimeStamp>> &);
    bool inTime(const TimeStamp &);
    void computeWeights(const std::vector<SparseSet<TimeStamp> > &validData, Visibilities *);

    std::vector<SparseSet<TimeStamp>> validData;
};

#endif
