#include "Common/Config.h"

#include "ISBI/OutputSection.h"
#include "Common/Affinity.h"

#include <omp.h>


OutputSection::OutputSection(const ISBI_Parset &ps)
:
  ps(ps),
  outputBuffers([&] () {
    if (ps.outputDescriptors().size() != ps.nrSubbands())
      throw Exception("expected the same amount of descriptors as the number of subbands");

    // FIXME: is it allowed to allocate a host buffer for devices[0] and use it on other devices?

    std::vector<std::unique_ptr<OutputBuffer>> buffers(ps.nrSubbands());

    for (unsigned subband = 0; subband < ps.nrSubbands(); subband ++) {
      std::unique_ptr<BoundThread> bt(ps.outputBufferNodes().size() > 0 ? new BoundThread(ps.allowedCPUs(ps.outputBufferNodes()[subband])) : nullptr);
      outputBuffers[subband] = std::unique_ptr<OutputBuffer>(new OutputBuffer(ps, subband));
    }

    return buffers;
  } ())
{
}


std::unique_ptr<Visibilities> OutputSection::getVisibilitiesBuffer(unsigned subband)
{
  return outputBuffers[subband]->getVisibilitiesBuffer();
}


void OutputSection::putVisibilitiesBuffer(std::unique_ptr<Visibilities> visibilities, const TimeStamp &time, unsigned subband)
{
  outputBuffers[subband]->putVisibilitiesBuffer(std::move(visibilities), time);
}
