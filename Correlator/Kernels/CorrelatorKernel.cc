#include "Common/Config.h"

#include <Common/Align.h>
#include "Correlator/Kernels/CorrelatorKernel.h"

#include <complex>
#include <iostream>
#include <algorithm>
#include <vector>

CorrelateSquareKernel::CorrelateSquareKernel(const CorrelatorParset &ps, cl::CommandQueue &queue, cl::Program &program, cl::Buffer &devVisibilities, cl::Buffer &devCorrectedData)
:
#if defined USE_2X2 || defined USE_3X3
  Kernel(program, "correlateSquareKernel")
#else
#error not implemented
#endif
{
  setArg(0, devVisibilities);
  setArg(1, devCorrectedData);

#if defined USE_2X2
  unsigned const nrStationsPerSquare = 32;
#elif defined USE_3X3
  unsigned const nrStationsPerSquare = 48;
#endif

  unsigned nrSquares = (ps.nrStations() / nrStationsPerSquare) * (ps.nrStations() / nrStationsPerSquare - 1) / 2;

#pragma omp critical (clog)
  std::clog << "nrSquares = " << nrSquares << std::endl;

  globalWorkSize = cl::NDRange(16 * 16, nrSquares, ps.nrOutputChannelsPerSubband());
  localWorkSize = cl::NDRange(16 * 16, 1, 1);

  nrOperations = (size_t) (nrStationsPerSquare * nrStationsPerSquare) * nrSquares * ps.nrOutputChannelsPerSubband() * ps.channelIntegrationFactor() * ps.nrSamplesPerChannel() * 8 * ps.nrVisibilityPolarizations();
  nrBytesRead = (size_t) (nrStationsPerSquare + nrStationsPerSquare) * nrSquares * ps.nrOutputChannelsPerSubband() * ps.channelIntegrationFactor() * ps.nrSamplesPerChannel() * ps.nrPolarizations() * sizeof(std::complex<float>);
  nrBytesWritten = (size_t) (nrStationsPerSquare * nrStationsPerSquare) * nrSquares * ps.nrOutputChannelsPerSubband() * ps.nrVisibilityPolarizations() * sizeof(std::complex<float>);
}


CorrelateRectangleKernel::CorrelateRectangleKernel(const CorrelatorParset &ps, cl::CommandQueue &queue, cl::Program &program, cl::Buffer &devVisibilities, cl::Buffer &devCorrectedData)
:
#if defined USE_2X2
  Kernel(program, "correlateRectangleKernel")
#else
#error not implemented
#endif
{
  setArg(0, devVisibilities);
  setArg(1, devCorrectedData);

  unsigned nrRectangles = ps.nrStations() % 32 == 0 ? 0 : ps.nrStations() / 32;
  unsigned nrThreads = align((32 / 2) * ((ps.nrStations() - 1) % 32 / 2 + 1), 64);

#pragma omp critical (clog)
  std::clog << "nrRectangles = " << nrRectangles << ", nrThreads = " << nrThreads << std::endl;

  globalWorkSize = cl::NDRange(nrThreads, nrRectangles, ps.nrOutputChannelsPerSubband());
  localWorkSize = cl::NDRange(nrThreads, 1, 1);

  nrOperations = (size_t) (32 * (ps.nrStations() % 32)) * nrRectangles * ps.nrOutputChannelsPerSubband() * ps.channelIntegrationFactor() * ps.nrSamplesPerChannel() * 8 * ps.nrVisibilityPolarizations();
  nrBytesRead = (size_t) (32 + ps.nrStations() % 32) * nrRectangles * ps.nrOutputChannelsPerSubband() * ps.channelIntegrationFactor() * ps.nrSamplesPerChannel() * ps.nrPolarizations() * sizeof(std::complex<float>);
  nrBytesWritten = (size_t) (32 * (ps.nrStations() % 32)) * nrRectangles * ps.nrOutputChannelsPerSubband() * ps.nrVisibilityPolarizations() * sizeof(std::complex<float>);
}


CorrelateTriangleKernel::CorrelateTriangleKernel(const CorrelatorParset &ps, cl::CommandQueue &queue, cl::Program &program, cl::Buffer &devVisibilities, cl::Buffer &devCorrectedData)
:
#if defined USE_2X2 || defined USE_3X3
  Kernel(program, "correlateTriangleKernel")
#else
#error not implemented
#endif
{
  setArg(0, devVisibilities);
  setArg(1, devCorrectedData);

#if defined USE_2X2
  unsigned const nrStationsPerTriangle = 32;
#elif defined USE_3X3
  unsigned const nrStationsPerTriangle = 48;
#endif
  unsigned nrTriangles = (ps.nrStations() + nrStationsPerTriangle - 1) / nrStationsPerTriangle;
  //unsigned nrMiniBlocksPerSide = 16;
  unsigned nrMiniBlocks = 256;//nrMiniBlocksPerSide * (nrMiniBlocksPerSide + 1) / 2;
  //size_t preferredMultiple;
  //getWorkGroupInfo(queue.getInfo<CL_QUEUE_DEVICE>(), CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, &preferredMultiple);
  //unsigned nrThreads = align(nrMiniBlocks, preferredMultiple);

#pragma omp critical (clog)
  std::clog << "nrTriangles = " << nrTriangles << ", nrThreads = " << nrMiniBlocks << std::endl;

  globalWorkSize = cl::NDRange(nrMiniBlocks, nrTriangles, ps.nrOutputChannelsPerSubband());
  localWorkSize = cl::NDRange(nrMiniBlocks, 1, 1);

  unsigned nrFullBlocks = ps.nrStations() / 32;
  unsigned loadForFullBlocks = 32 * 32 * nrFullBlocks;
  unsigned loadForPartialBlocks = (ps.nrStations() % 32) * (ps.nrStations() % 32);

  nrOperations = (size_t) (loadForFullBlocks + loadForPartialBlocks) * ps.nrOutputChannelsPerSubband() * ps.channelIntegrationFactor() * ps.nrSamplesPerChannel() * 4 * ps.nrVisibilityPolarizations();
  nrBytesRead = (size_t) ps.nrStations() * ps.nrOutputChannelsPerSubband() * ps.channelIntegrationFactor() * ps.nrSamplesPerChannel() * ps.nrPolarizations() * sizeof(std::complex<float>);
  nrBytesWritten = (size_t) (loadForFullBlocks + loadForPartialBlocks) * ps.nrOutputChannelsPerSubband() * ps.nrVisibilityPolarizations() * sizeof(std::complex<float>) / 2;
}
