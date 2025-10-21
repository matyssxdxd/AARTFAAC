#include "Common/Config.h"
#include "Common/PowerSensor.h"
#include "Correlator/CorrelatorPipeline.h"
#include "Correlator/DeviceInstance.h"

#include <cudawrappers/nvrtc.hpp>

#include <cuda_fp16.h>
#include <omp.h>

#include <iostream>
#include <cmath>

#undef ISBI_DELAYS
#undef ISBI_FIR

#if 0 && defined CL_DEVICE_TOPOLOGY_AMD
inline static cpu_set_t cpu_and(const cpu_set_t &a, const cpu_set_t &b)
{
  cpu_set_t c;
  CPU_AND(&c, &a, &b);
  return c;
}
#endif


extern const char _binary_Correlator_Kernels_Transpose_cu_start, _binary_Correlator_Kernels_Transpose_cu_end;


DeviceInstance::DeviceInstance(CorrelatorPipeline &pipeline, unsigned deviceNr)
:
  pipeline(pipeline),
  ps(pipeline.ps),

#if 0 && defined CL_DEVICE_TOPOLOGY_AMD
  supportNumaAMD(device.getInfo<CL_DEVICE_VENDOR_ID>() == 0x1002),
  numaNode(supportNumaAMD ? getNUMAnodeOfPCIeDevice(device.getInfo<CL_DEVICE_TOPOLOGY_AMD>().pcie.bus & 255,
						    device.getInfo<CL_DEVICE_TOPOLOGY_AMD>().pcie.device & 255,
						    device.getInfo<CL_DEVICE_TOPOLOGY_AMD>().pcie.function & 255) : 0),
  boundThread(supportNumaAMD ? new BoundThread(ps.allowedCPUs(numaNode)) : nullptr),
//#else
  //numaNode(0),
#endif

  device(deviceNr),
  context(CU_CTX_SCHED_BLOCKING_SYNC, device),
  //integratedMemory(device.getAttribute(CU_DEVICE_ATTRIBUTE_INTEGRATED)),

  filterOddFuture(std::async([&] {
    context.setCurrent();

    tcc::FilterArgs filterOddArgs;
    filterOddArgs.nrReceivers = ps.nrStations();
    filterOddArgs.nrChannels = ps.nrChannelsPerSubband();
    filterOddArgs.nrSamplesPerChannel = ps.nrSamplesPerChannelAfterFilter();
    filterOddArgs.nrPolarizations = ps.nrPolarizations();

    filterOddArgs.input = tcc::FilterArgs::Input {
      .sampleFormat = tcc::FilterArgs::Format::i16,
      .isPurelyReal = true
    };

#ifdef ISBI_FIR
    filterOddArgs.firFilter = tcc::FilterArgs::FIR_Filter {
      .nrTaps = 16,
      .sampleFormat = tcc::FilterArgs::Format::fp32
    };
#else
    filterOddArgs.firFilter = std::nullopt;
#endif

    filterOddArgs.fft = tcc::FilterArgs::FFT {
      .sampleFormat = tcc::FilterArgs::Format::fp32,
      .shift = false,
      .mirror = true 
    };

#ifdef ISBI_DELAYS
    filterOddArgs.delays = tcc::FilterArgs::Delays {
      .subbandBandwidth = ps.subbandBandwidth(),
      .polynomialOrder = 1,
      .separatePerPolarization = false
    };
#endif

    filterOddArgs.output = tcc::FilterArgs::Output {
      .sampleFormat = tcc::FilterArgs::Format::fp16,
      .scaleFactor = std::nullopt
    };

    return tcc::Filter(device, filterOddArgs);
  })),

  filterFuture(std::async([&] {
    context.setCurrent();
     
    tcc::FilterArgs filterArgs;
    filterArgs.nrReceivers = ps.nrStations();
    filterArgs.nrChannels = ps.nrChannelsPerSubband();
    filterArgs.nrSamplesPerChannel = ps.nrSamplesPerChannelAfterFilter();
    filterArgs.nrPolarizations = ps.nrPolarizations();

    filterArgs.input = tcc::FilterArgs::Input {
	.sampleFormat = tcc::FilterArgs::Format::i16,
	.isPurelyReal = true
    };

#ifdef ISBI_FIR
    filterArgs.firFilter = tcc::FilterArgs::FIR_Filter {
    	.nrTaps = 16,
        .sampleFormat = tcc::FilterArgs::Format::fp32
    };
#else
    filterArgs.firFilter = std::nullopt;
#endif

    filterArgs.fft = tcc::FilterArgs::FFT {
    	.sampleFormat = tcc::FilterArgs::Format::fp32,
        .shift = false,
        .mirror = false 
    };

#ifdef ISBI_DELAYS 
    filterArgs.delays = {
      .subbandBandwidth = ps.subbandBandwidth(),
      .polynomialOrder = 1,
      .separatePerPolarization = false
    };
#endif
    
    filterArgs.output = tcc::FilterArgs::Output {
      .sampleFormat = tcc::FilterArgs::Format::fp16,
      .scaleFactor = std::nullopt
    };


    return tcc::Filter(device, filterArgs);
  })),

  tccFuture(std::async([&] {
    context.setCurrent();
    return TCC(device, ps);
  })),
  
  devCorrectedData((size_t) ps.nrChannelsPerSubband() * ps.nrSamplesPerChannelAfterFilter() * ps.nrStations() * ps.nrPolarizations() * ps.nrBytesPerComplexSample()),
  
  filterOdd(filterOddFuture.get()),
  filter(filterFuture.get()),
  tcc(tccFuture.get()),

  previousTime(~0)
{
#if 0 && defined CL_DEVICE_TOPOLOGY_AMD
  if (supportNumaAMD) {
    unsigned bus = device.getInfo<CL_DEVICE_TOPOLOGY_AMD>().pcie.bus & 255;

#pragma omp critical (clog)
    std::clog << "GPU on bus " << bus << " is on NUMA node " << numaNode << std::endl;

    switch (bus) {
      case 0x06:
      case 0x07:  lockNumber = 0;
		  break;

      case 0x0D:
      case 0x0E:
      case 0x11:
      case 0x12:  lockNumber = 1;
		  break;

      case 0x85:
      case 0x86:
      case 0x87:
      case 0x88:
      case 0x89:
      case 0x8A:	
      case 0x8B:  lockNumber = 2;
		  break;

      default:	  std::clog << "Warning: device at unknown bus number " << bus << std::endl;
    		  lockNumber = 3;
    }
  }
#endif

  // do not wait for these events during first iteration, so generate them
  // already once during initialization
  //executeStream.record(inputDataFree);

  executeStream.synchronize();

#if defined CL_DEVICE_TOPOLOGY_AMD
  boundThread = nullptr;
#endif
}


DeviceInstanceWithoutUnifiedMemory::DeviceInstanceWithoutUnifiedMemory(CorrelatorPipeline &pipeline, unsigned deviceNr)
:
  DeviceInstance(pipeline, deviceNr),
  devInputBuffer((size_t) ps.nrStations() * ps.nrPolarizations() * (ps.nrSamplesPerChannelBeforeFilter() + NR_TAPS - 1) * ps.nrChannelsPerSubband() * ps.nrBytesPerRealSample()),
  devDelaysAtBegin(ps.nrBeams() * ps.nrStations() * ps.nrPolarizations() * sizeof(float)),
  devDelaysAfterEnd(ps.nrBeams() * ps.nrStations() * ps.nrPolarizations() * sizeof(float)),
  devFracDelays(sizeof(float) * 2 * 2),
  currentVisibilityBuffer(0)
{
  for (unsigned buffer = 0; buffer < NR_DEV_VISIBILITIES_BUFFERS; buffer ++)
    deviceToHostStream.record(visibilityDataFree[buffer]);

  for (unsigned i = 0; i < NR_DEV_VISIBILITIES_BUFFERS; i ++)
    devVisibilities.emplace_back(cu::DeviceMemory((size_t) ps.nrOutputChannelsPerSubband() * ps.nrBaselines() * ps.nrVisibilityPolarizations() * sizeof(std::complex<float>)));
}


DeviceInstance::~DeviceInstance()
{
  context.setCurrent();
}


#if 0 && defined CL_DEVICE_TOPOLOGY_AMD
// avoid that multiple GPUs share PCIe bandwidth

static std::mutex hostToDeviceLock[4], deviceToHostLock[4];

static void lockPCIeBus(cl_event event, cl_int status, void *arg)
{
  static_cast<std::mutex *>(arg)->lock();
}


static void unlockPCIeBus(cl_event event, cl_int status, void *arg)
{
  static_cast<std::mutex *>(arg)->unlock();
}

#endif


void DeviceInstance::doSubband(const TimeStamp &time,
			       unsigned subband,
			       std::function<void (cu::Stream &, cu::DeviceMemory &devInputBuffer, PerformanceCounter &)> &enqueueHostToDeviceTransfer,
			       const MultiArrayHostBuffer<char, 4> &hostInputBuffer,
			       const MultiArrayHostBuffer<float, 3> &hostDelaysAtBegin,
			       const MultiArrayHostBuffer<float, 3> &hostDelaysAfterEnd,
			       MultiArrayHostBuffer<std::complex<float>, 3> &hostVisibilities,
			       unsigned startIndex
			      )
{
  context.setCurrent();

  {
    std::lock_guard<std::mutex> lock(enqueueMutex);

    std::cout << "DeviceInstance:doSubband\n";
    filter.launchAsync(executeStream,
		         devCorrectedData,
			 cu::DeviceMemory(hostInputBuffer));

    cu::DeviceMemory devVisibilities(hostVisibilities);
    cu::DeviceMemory devCorrectedDataChannel0skipped(static_cast<CUdeviceptr>(devCorrectedData) + ps.nrSamplesPerChannelAfterFilter() * ps.nrStations() * ps.nrPolarizations() * ps.nrBytesPerComplexSample());
    tcc.launchAsync(executeStream, devVisibilities, devCorrectedDataChannel0skipped, pipeline.correlateCounter);
  }

  executeStream.synchronize();
}


void DeviceInstanceWithoutUnifiedMemory::doSubband(const TimeStamp &time,
				   unsigned subband,
				   std::function<void (cu::Stream &, cu::DeviceMemory &devInputBuffer, PerformanceCounter &)> &enqueueHostToDeviceTransfer,
				   const MultiArrayHostBuffer<char, 4> &hostInputBuffer,
				   const MultiArrayHostBuffer<float, 3> &hostDelaysAtBegin,
				   const MultiArrayHostBuffer<float, 3> &hostDelaysAfterEnd,
				   MultiArrayHostBuffer<std::complex<float>, 3> &hostVisibilities,
				   unsigned startIndex
				  )
{
  context.setCurrent();

  cu::Event inputTransferReady, computeReady, visibilityTransferReady;

  {
    std::lock_guard<std::mutex> lock(enqueueMutex);

    // TODO: input samples and delays could be copied at different times.

    hostToDeviceStream.wait(inputDataFree);

    if (time != previousTime) {
      // only transfer delays if time has changed
      previousTime = time;

      hostToDeviceStream.memcpyHtoDAsync(devDelaysAtBegin, hostDelaysAtBegin.origin(), hostDelaysAtBegin.bytesize());
      hostToDeviceStream.memcpyHtoDAsync(devDelaysAfterEnd, hostDelaysAfterEnd.origin(), hostDelaysAfterEnd.bytesize());
    
#ifdef ISBI_DELAYS
      uint32_t blockSize = ps.nrSamplesPerChannelBeforeFilter() * ps.nrChannelsPerSubband();
      uint64_t timeOffset = time - ps.startTime();
      uint64_t totalTimeRange = ps.stopTime() - ps.startTime();
      uint32_t N = static_cast<uint32_t>(totalTimeRange / blockSize);
      uint32_t i = std::min(static_cast<uint32_t>((timeOffset / blockSize)), N - 1);

      float hostFracDelays[2][2];

      double delayInSamples = ps.delays()[i] * ps.subbandBandwidth();
      int integerDelay = static_cast<int>(std::floor(delayInSamples + .5));
      float fractionalDelay = static_cast<float>(delayInSamples - integerDelay);

      double delayInSamples_N = ps.delays()[i + 1] * ps.subbandBandwidth();
      int integerDelay_N = static_cast<int>(std::floor(delayInSamples + .5));
      float dN = static_cast<float>(delayInSamples_N - integerDelay_N);

      // In this case the antenna 0 is chosen as a reference antenna
      hostFracDelays[1][0] = fractionalDelay; 
      hostFracDelays[1][1] = (dN - fractionalDelay) / ps.nrSamplesPerChannelBeforeFilter();
      hostFracDelays[0][0] = 0;
      hostFracDelays[0][1] = 0;

      hostToDeviceStream.memcpyHtoDAsync(devFracDelays, hostFracDelays, sizeof(float) * 2 * 2); 
#endif
    }

#ifdef ISBI_DELAYS
    double subbandCenterFrequency = ps.centerFrequencies()[subband] * 1e6;
#endif
    enqueueHostToDeviceTransfer(hostToDeviceStream, devInputBuffer, pipeline.samplesCounter);
    hostToDeviceStream.record(inputTransferReady);

#if 0 && defined CL_DEVICE_TOPOLOGY_AMD
    if (supportNumaAMD) {
      inputTransferStarted[0].setCallback(CL_SUBMITTED, &lockPCIeBus, &hostToDeviceLock[lockNumber]);
      inputTransferReady[0].setCallback(CL_RUNNING, &unlockPCIeBus, &hostToDeviceLock[lockNumber]);
    }
#endif

    //pipeline.samplesCounter.doOperation(inputTransferReady[0], 0, 0, bytesSent);

    executeStream.wait(inputTransferReady);

    if (((subband + 1) % 2) == 0) {
#ifdef ISBI_DELAYS
      filter.launchAsync(executeStream,
          devCorrectedData,
          devInputBuffer,
          devFracDelays,
          subbandCenterFrequency);
#else
      filter.launchAsync(executeStream,
          devCorrectedData,
          devInputBuffer);
#endif
    } else {
#ifdef ISBI_DELAYS
      filterOdd.launchAsync(executeStream,
          devCorrectedData,
          devInputBuffer,
          devFracDelays,
          subbandCenterFrequency);
#else
      filterOdd.launchAsync(executeStream,
          devCorrectedData,
          devInputBuffer);
#endif
    }

    executeStream.record(inputDataFree);
    executeStream.wait(visibilityDataFree[currentVisibilityBuffer]);

#ifdef DEBUG_PHASE_DUMP
    unsigned nrTimesPerBlock = 128 / ps.nrBitsPerSample();
    MultiArrayHostBuffer<std::complex<__half>, 5> hostCorrectedData(boost::extents[ps.nrOutputChannelsPerSubband()][ps.nrSamplesPerChannelAfterFilter() * ps.channelIntegrationFactor() / nrTimesPerBlock][ps.nrStations()][ps.nrPolarizations()][nrTimesPerBlock]);
    executeStream.memcpyDtoHAsync(hostCorrectedData.origin(), devCorrectedData, (size_t) ps.nrOutputChannelsPerSubband() * ps.nrSamplesPerChannelAfterFilter() * ps.channelIntegrationFactor() * ps.nrStations() * ps.nrPolarizations() * ps.nrBytesPerComplexSample());
    executeStream.synchronize();

    std::cout << "=== Phase Dump: Time=" << time << ", Subband=" << subband << " ===\n";
    
    unsigned dumpCount = 0;
    const unsigned maxDumps = 1000;

    for (unsigned outputChannel = 0; outputChannel < ps.nrOutputChannelsPerSubband(); outputChannel ++)
      for (unsigned timeMajor = 0; timeMajor < ps.nrSamplesPerChannelAfterFilter() * ps.channelIntegrationFactor() / nrTimesPerBlock; timeMajor ++)
        for (unsigned receiver = 0; receiver < ps.nrStations(); receiver ++)
          for (unsigned polarization = 0; polarization < ps.nrPolarizations(); polarization ++)
            for (unsigned timeMinor = 0; timeMinor < nrTimesPerBlock; timeMinor ++) {
              std::complex<__half> sample = hostCorrectedData[outputChannel][timeMajor][receiver][polarization][timeMinor];

              if (sample != std::complex<__half>(0.0f) && dumpCount < maxDumps) {
                float real = __half2float(sample.real());
                float imag = __half2float(sample.imag());
                float phase = std::atan2(imag, real);
                float magnitude = std::sqrt(real*real + imag*imag);
                
                std::cout << "COR[" << outputChannel << "][" << timeMajor
                  << "][" << receiver << "][" << polarization
                  << "][" << timeMinor << "] = ("
                  << real << "," << imag << ") phase=" << phase 
                  << " mag=" << magnitude << "\n";
                dumpCount++;
              }
            }

    std::cout << "=== Dumped " << dumpCount << " samples ===\n";

    exit(0);
#endif

    cu::DeviceMemory devCorrectedDataChannel0skipped(static_cast<CUdeviceptr>(devCorrectedData) + ps.nrSamplesPerChannelAfterFilter() * ps.nrStations() * ps.nrPolarizations() * ps.nrBytesPerComplexSample());
    tcc.launchAsync(executeStream, devVisibilities[currentVisibilityBuffer], devCorrectedDataChannel0skipped, pipeline.correlateCounter);
    
    executeStream.record(computeReady);
    deviceToHostStream.wait(computeReady);

    {
      PerformanceCounter::Measurement measurement(pipeline.visibilitiesCounter, deviceToHostStream, 0, hostVisibilities.bytesize(), 0);
      deviceToHostStream.memcpyDtoHAsync(hostVisibilities.origin(), devVisibilities[currentVisibilityBuffer], hostVisibilities.bytesize());
    }

    deviceToHostStream.record(visibilityTransferReady);

#if 0 && defined CL_DEVICE_TOPOLOGY_AMD
    if (supportNumaAMD) {
      visibilityTransferReady.setCallback(CL_SUBMITTED, &lockPCIeBus, &deviceToHostLock[lockNumber]);
      visibilityTransferReady.setCallback(CL_RUNNING, &unlockPCIeBus, &deviceToHostLock[lockNumber]);
    }
#endif

    deviceToHostStream.record(visibilityDataFree[currentVisibilityBuffer]);
  }

  if (++ currentVisibilityBuffer == NR_DEV_VISIBILITIES_BUFFERS)
    currentVisibilityBuffer = 0;

  visibilityTransferReady.synchronize();

#if 0
  for (unsigned baseline = 0, count = 0; baseline < ps.nrBaselines(); baseline ++)
    for (unsigned channel = 0; channel < ps.nrOutputChannelsPerSubband(); channel ++)
      for (unsigned polarization = 0; polarization < ps.nrVisibilityPolarizations(); polarization ++)
	//if (hostVisibilities[baseline][channel][polarization] != hostVisibilities[channel][baseline][polarization])
	if (hostVisibilities[baseline][channel][polarization] != 0.0f) {
	  std::cout << "vis[" << baseline << "][" << channel << "][" << polarization << "] = " << hostVisibilities[baseline][channel][polarization] << ' ' << conj(hostVisibilities[baseline][channel][polarization]) << std::endl;

	  if (++ count == 10000)
	    exit(0);
	}
#endif
}


void DeviceInstance::doSubband(const TimeStamp &time,
			       unsigned subband,
			       const MultiArrayHostBuffer<char, 4> &hostInputBuffer,
			       const MultiArrayHostBuffer<float, 3> &hostDelaysAtBegin,
			       const MultiArrayHostBuffer<float, 3> &hostDelaysAfterEnd,
			       MultiArrayHostBuffer<std::complex<float>, 3> &hostVisibilities
			      )
{
  std::function<void (cu::Stream &, cu::DeviceMemory &, PerformanceCounter &)> enqueueHostToDeviceTransfer = [&] (cu::Stream &stream, cu::DeviceMemory &devInputBuffer, PerformanceCounter &counter) {
    PerformanceCounter::Measurement measurement(counter, stream, 0, 0, hostInputBuffer.bytesize());
    stream.memcpyHtoDAsync(devInputBuffer, hostInputBuffer, hostInputBuffer.bytesize());
  };

  doSubband(time, subband, enqueueHostToDeviceTransfer, hostInputBuffer, hostDelaysAtBegin, hostDelaysAfterEnd, hostVisibilities);
}
