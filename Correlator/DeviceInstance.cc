#include "Common/Config.h"
#include "Common/PowerSensor.h"
#include "Correlator/CorrelatorPipeline.h"
#include "Correlator/DeviceInstance.h"

#include <cudawrappers/nvrtc.hpp>

#include <cuda_fp16.h>
#include <omp.h>

#include <iostream>
#include <cmath>

#define ISBI_DELAYS

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
    filterOddArgs.nrSamplesPerChannel = ps.nrSamplesPerChannel();
    filterOddArgs.nrPolarizations = ps.nrPolarizations();

    filterOddArgs.input = tcc::FilterArgs::Input {
      .sampleFormat = tcc::FilterArgs::Format::i8,
      .isPurelyReal = true
    };

    filterOddArgs.firFilter = tcc::FilterArgs::FIR_Filter {
      .nrTaps = 16,
      .sampleFormat = tcc::FilterArgs::Format::fp32
    };

    filterOddArgs.fft = tcc::FilterArgs::FFT {
      .sampleFormat = tcc::FilterArgs::Format::fp32,
      .shift = false,
      .mirror = false 
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
    filterArgs.nrSamplesPerChannel = ps.nrSamplesPerChannel();
    filterArgs.nrPolarizations = ps.nrPolarizations();

    filterArgs.input = tcc::FilterArgs::Input {
	.sampleFormat = tcc::FilterArgs::Format::i8,
	.isPurelyReal = true
    };

    filterArgs.firFilter = tcc::FilterArgs::FIR_Filter {
    	.nrTaps = 16,
        .sampleFormat = tcc::FilterArgs::Format::fp32
    };

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

  devCorrectedData((size_t) ps.nrChannelsPerSubband() * ps.nrSamplesPerChannel() * ps.nrStations() * ps.nrPolarizations() * ps.nrBytesPerComplexSample()),
  
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
  devInputBuffer((size_t) ps.nrStations() * ps.nrPolarizations() * (ps.nrSamplesPerChannel() + NR_TAPS - 1) * ps.nrChannelsPerSubbandBeforeFilter() * ps.nrBytesPerRealSample()),
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
    cu::DeviceMemory devCorrectedDataChannel0skipped(static_cast<CUdeviceptr>(devCorrectedData) + ps.nrSamplesPerChannel() * ps.nrStations() * ps.nrPolarizations() * ps.nrBytesPerComplexSample());
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
    
      uint32_t blockSize = ps.nrSamplesPerSubbandBeforeFilter();
      uint64_t timeOffset = time - ps.startTime();
      uint64_t totalTimeRange = ps.stopTime() - ps.startTime();
      uint32_t N = static_cast<uint32_t>(totalTimeRange / blockSize);
      uint32_t i = std::min(static_cast<uint32_t>((timeOffset / blockSize)), N - 1);

      float hostFracDelays[2][2];

      double delayInSamples = ps.delays()[i] * ps.sampleRate();
      int integerDelay = static_cast<int>(std::floor(delayInSamples + .5));
      double fractionalDelayInSamples = delayInSamples - integerDelay;

      // Convert fractional delay from samples to SECONDS
      float fractionalDelayInSeconds = static_cast<float>(fractionalDelayInSamples / ps.sampleRate());
      
      // Delay rate in SECONDS per output sample
      // ps.delays() is already in seconds, so:
      float delayRate = static_cast<float>((ps.delays()[i + 1] - ps.delays()[i]) / ps.nrSamplesPerChannel());
      
      std::cout << "i=" << i << ",N=" << N << std::endl;
      std::cout << "delay_in_samples=" << delayInSamples << std::endl;
      std::cout << "integer_delay=" << integerDelay << std::endl;
      std::cout << "fractional_delay_seconds=" << fractionalDelayInSeconds << std::endl;
      std::cout << "delay_rate_seconds_per_sample=" << delayRate << std::endl;
      
      // In this case the antenna 0 is chosen as a reference antenna
      hostFracDelays[1][0] = fractionalDelayInSeconds;  // seconds
      hostFracDelays[1][1] = delayRate;                  // seconds per output sample
      hostFracDelays[0][0] = 0;
      hostFracDelays[0][1] = 0;

      hostToDeviceStream.memcpyHtoDAsync(devFracDelays, hostFracDelays, sizeof(float) * 2 * 2);
    }

    double subbandCenterFrequency = ps.centerFrequencies()[subband];
     
    enqueueHostToDeviceTransfer(hostToDeviceStream, devInputBuffer, pipeline.samplesCounter);

#if 0
    uint32_t n = ps.nrSamplesPerSubbandBeforeFilter() + (NR_TAPS - 1) * ps.nrChannelsPerSubband();
    size_t nrBytesPerTime = ps.nrBytesPerRealSample();

    MultiArrayHostBuffer<char, 4> hostData(boost::extents
        [ps.nrStations()]
        [ps.nrPolarizations()]
        [n]
        [nrBytesPerTime]);

    executeStream.memcpyDtoHAsync(hostData.origin(), devInputBuffer, 
        (size_t) ps.nrStations() * ps.nrPolarizations() * n * nrBytesPerTime);
    executeStream.synchronize();

    for (unsigned polarization = 0; polarization < ps.nrPolarizations(); polarization++) {
      for (unsigned sample = 0; sample < n; sample++) {
        int32_t station_0 = static_cast<int32_t>(*reinterpret_cast<const int8_t*>(&hostData[0][polarization][sample][0]));
        int32_t station_1 = static_cast<int32_t>(*reinterpret_cast<const int8_t*>(&hostData[1][polarization][sample][0]));

        if (station_0 != station_1)
          std::cout << "Polarization: " << polarization << " Sample: " << sample << " Station_0: " << station_0 << " Station_1: " << station_1 << std::endl;

      }
    }
#endif

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
      filter.launchAsync(executeStream,
          devCorrectedData,
          devInputBuffer,
          devFracDelays,
          subbandCenterFrequency);
    } else {
      filterOdd.launchAsync(executeStream,
          devCorrectedData,
          devInputBuffer,
          devFracDelays,
          subbandCenterFrequency);
    }

    executeStream.record(inputDataFree);
    executeStream.wait(visibilityDataFree[currentVisibilityBuffer]);

#if 0
    unsigned nrTimesPerBlock = 128 / ps.nrBitsPerSample();
    MultiArrayHostBuffer<std::complex<half>, 5> hostCorrectedData(boost::extents[ps.nrOutputChannelsPerSubband()][ps.nrSamplesPerChannelAfterFilter() * ps.channelIntegrationFactor() / nrTimesPerBlock][ps.nrStations()][ps.nrPolarizations()][nrTimesPerBlock]);
    executeStream.memcpyDtoHAsync(hostCorrectedData.origin(), devCorrectedData, (size_t) ps.nrOutputChannelsPerSubband() * ps.nrSamplesPerChannelAfterFilter() * ps.channelIntegrationFactor() * ps.nrStations() * ps.nrPolarizations() * ps.nrBytesPerComplexSample());
    executeStream.synchronize();

    std::cout << "[outputChannel][timeMajor][receiver][polarization][timeMinor]" << std::endl;

    for (unsigned outputChannel = 0; outputChannel < ps.nrOutputChannelsPerSubband(); outputChannel ++)
      for (unsigned timeMajor = 0; timeMajor < 500; timeMajor ++)
        // for (unsigned receiver = 0; receiver < ps.nrStations(); receiver ++)
        for (unsigned polarization = 0; polarization < ps.nrPolarizations(); polarization ++)
          for (unsigned timeMinor = 0; timeMinor < nrTimesPerBlock; timeMinor ++) {
            std::complex<half> sample_1 = hostCorrectedData[outputChannel][timeMajor][0][polarization][timeMinor];
            std::complex<half> sample_2 = hostCorrectedData[outputChannel][timeMajor][1][polarization][timeMinor];

              std::cout << "COR[" << outputChannel << "][" << timeMajor << "][" << 0 
                << "][" << polarization << "][" << timeMinor << "] = ("
                << static_cast<float>(sample_1.real()) << ","
                << static_cast<float>(sample_1.imag()) << ")";

              std::cout << "=== COR[" << outputChannel << "][" << timeMajor << "][" << 1 
                << "][" << polarization << "][" << timeMinor << "] = ("
                << static_cast<float>(sample_2.real()) << ","
                << static_cast<float>(sample_2.imag()) << ")" << std::endl;
          }

    exit(0);
#endif

    cu::DeviceMemory devCorrectedDataChannel0skipped(static_cast<CUdeviceptr>(devCorrectedData) + ps.nrSamplesPerChannel() * ps.nrStations() * ps.nrPolarizations() * ps.nrBytesPerComplexSample());
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
