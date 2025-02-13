#include "Common/Config.h"

#include "AARTFAAC/OutputBuffer.h"
#include "Common/Affinity.h"
#include "Common/Stream/Descriptor.h"
#include "Common/Stream/SocketStream.h"

#include <iostream>


OutputBuffer::OutputBuffer(const AARTFAAC_Parset &ps, unsigned subband)
:
  ps(ps),
  subband(subband),
  currentTime(ps.startTime()),
  stream(createStream(ps.outputDescriptors()[subband], false)),
  thread(&OutputBuffer::outputThreadBody, this)
{
  SocketStream *socketStream = dynamic_cast<SocketStream *>(stream.get());

  if (socketStream != nullptr)
    socketStream->setWriteBufferSize(64 * 1024 * 1024);

  Visibilities *vis;

  //for (unsigned i = 0; i < 3; i ++) { Does not fit on A100
  for (unsigned i = 0; i < 2; i ++) {
    std::unique_ptr<Visibilities> visibilties(vis = new Visibilities(ps, subband));
    freeQueue.append(visibilties);
  }

#pragma omp critical (clog)
  std::clog << "output buffer " << subband << " created by CPU " << currentCPU() << " on node " << currentNode() << ", memory at node " << node(vis->hostVisibilities.data()) << std::endl;
}


OutputBuffer::~OutputBuffer()
{
  std::unique_ptr<Visibilities> terminator(nullptr);
  pendingQueue.append(terminator);

  thread.join();
}


void OutputBuffer::outputThreadBody()
{
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
  try {
#endif
    std::unique_ptr<Visibilities> visibilities, integratedVisibilities;

    while ((integratedVisibilities = pendingQueue.remove()) != nullptr) {
      for (unsigned i = 1; i < ps.visibilitiesIntegration(); i ++) {
	if ((visibilities = pendingQueue.remove()) == nullptr)
	  return;

	*integratedVisibilities += *visibilities;
	freeQueue.append(visibilities);
      }

//#pragma omp critical (writelock)
      integratedVisibilities->write(stream.get());
      freeQueue.append(integratedVisibilities);
    }
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
  } catch (std::exception &ex) {
#pragma omp critical (cerr)
    std::cerr << "caught std::exception: " << ex.what() << std::endl;
  }
#endif
}


std::unique_ptr<Visibilities> OutputBuffer::getVisibilitiesBuffer()
{
  if (!freeQueue.empty() || !ps.realTime())
    return freeQueue.remove();

#pragma omp critical (clog)
  std::clog << "Warning: dropping visibilities block for subband " << subband << std::endl;
  return pendingQueue.remove();
}


void OutputBuffer::putVisibilitiesBuffer(std::unique_ptr<Visibilities> visibilities, const TimeStamp &time)
{
  currentTime.waitFor(time); // make sure that visibilities are ordered

#if 0
  if (visibilities != nullptr)
    for (unsigned baseline = 0; baseline < ps.nrBaselines(); baseline ++)
      for (unsigned channel = 0; channel < ps.nrOutputChannelsPerSubband(); channel ++)
	for (unsigned polarization = 0; polarization < ps.nrVisibilityPolarizations(); polarization ++)
	  if ((visibilities->visibilities)[baseline][channel][polarization] != std::complex<float>(0, 0))
#pragma omp critical (cout)
	    std::cout << "bl = " << baseline << ", ch = " << channel << ", pol = " << polarization << ": " << (visibilities->visibilities)[baseline][channel][polarization] << std::endl;
#endif

  if (visibilities != nullptr) // visibilities == nullptr ==> skipped block
    pendingQueue.append(visibilities);

  currentTime.advanceTo(time + ps.nrSamplesPerSubbandAfterFilter());
}
