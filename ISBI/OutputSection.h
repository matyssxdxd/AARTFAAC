#ifndef ISBI_OUTPUT_SECTION_H
#define ISBI_OUTPUT_SECTION_H

#include "ISBI/Parset.h"
#include "ISBI/OutputBuffer.h"
#include "ISBI/Visibilities.h"
#include "Common/TimeStamp.h"

#include <vector>


class OutputSection
{
  public:
    OutputSection(const ISBI_Parset &);

    std::unique_ptr<Visibilities> getVisibilitiesBuffer(unsigned subband);
    void putVisibilitiesBuffer(std::unique_ptr<Visibilities>, const TimeStamp &, unsigned subband);

  private:
    const ISBI_Parset &ps;
    std::vector<std::unique_ptr<OutputBuffer>> outputBuffers;
};

#endif
