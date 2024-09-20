#ifndef OUTPUT_SECTION_H
#define OUTPUT_SECTION_H

#include "AARTFAAC/Parset.h"
#include "AARTFAAC/OutputBuffer.h"
#include "AARTFAAC/Visibilities.h"
#include "Common/TimeStamp.h"

#include <vector>


class OutputSection
{
  public:
    OutputSection(const AARTFAAC_Parset &);

    std::unique_ptr<Visibilities> getVisibilitiesBuffer(unsigned subband);
    void putVisibilitiesBuffer(std::unique_ptr<Visibilities>, const TimeStamp &, unsigned subband);

  private:
    const AARTFAAC_Parset &ps;
    std::vector<std::unique_ptr<OutputBuffer>> outputBuffers;
};

#endif
