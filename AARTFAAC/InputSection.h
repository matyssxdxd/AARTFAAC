#ifndef INPUT_SECTION_H
#define INPUT_SECTION_H

#include "AARTFAAC/Parset.h"
#include "AARTFAAC/InputBuffer.h"
#include "Common/CUDA_Support.h"
#include "Common/PerformanceCounter.h"
#include "Common/SparseSet.h"
#include "Common/TimeStamp.h"

#include <vector>


class InputSection
{
  public:
    InputSection(const AARTFAAC_Parset &);
    ~InputSection();
    
    void fillInMissingSamples(const TimeStamp &, unsigned subband, std::vector<SparseSet<TimeStamp> > &validData);
    void enqueueHostToDeviceCopy(cu::Stream &, cu::DeviceMemory &devBuffer, PerformanceCounter &, const TimeStamp &, unsigned subband);

    void startReadTransaction(const TimeStamp &);
    void endReadTransaction(const TimeStamp &);

  private:
    const AARTFAAC_Parset &ps;
  
  public:
    std::vector<MultiArrayHostBuffer<char, 4>> hostRingBuffers;

  private:
    std::vector<std::unique_ptr<InputBuffer>> inputBuffers;

    const static unsigned nrTimesPerPacket = 2;
    const static unsigned nrDipolesPerStation = 48;

    unsigned nrRingBufferSamplesPerSubband;
};

#endif
