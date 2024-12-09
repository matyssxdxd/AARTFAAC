#include "Common/Align.h"
#include "Correlator/Filter.h"
#include "Common/FilterBank.h"

#include <cudawrappers/nvrtc.hpp>

#include <mathdx-helper.hpp>

#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <vector>

#define GNU_SOURCE
#include <link.h>


extern const char _binary_libfilter_kernel_FilterAndCorrect_cu_start, _binary_libfilter_kernel_FilterAndCorrect_cu_end;


namespace tcc
{

cu::Module Filter::compileModule(const cu::Device &device)
{
  int capability = 10 * device.getAttribute<CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR>() + device.getAttribute<CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR>();

  std::vector<std::string> options =
  {
    "-default-device", // necessary to compile cuFFTDx through NVRTC
    "-std=c++17",
    "-arch=compute_" + std::to_string(capability),
    "-lineinfo",
    "-DNR_RECEIVERS=" + std::to_string(nrReceivers),
    "-DNR_CHANNELS=" + std::to_string(nrChannels),
    "-DNR_CHANNELS_PER_THREAD=" + std::to_string(nrChannelsPerThread),
    "-DNR_TIMES_PER_ITERATION=" + std::to_string(nrTimesPerIteration),
    "-DNR_SAMPLES_PER_CHANNEL=" + std::to_string(nrSamplesPerChannel),
    "-DNR_POLARIZATIONS=" + std::to_string(nrPolarizations),
    "-DINPUT_SAMPLE_FORMAT=" + std::to_string((int) input.sampleFormat),
    "-DFFT_SAMPLE_FORMAT=" + std::to_string(fft.sampleFormat),
    "-DOUTPUT_SAMPLE_FORMAT=" + std::to_string(output.sampleFormat),
    "-DFP32=" + std::to_string(fp32),
    "-DFP16=" + std::to_string(fp16),
    "-DBF16=" + std::to_string(bf16),
    "-DE4M3=" + std::to_string(e4m3),
    "-DE5M2=" + std::to_string(e5m2),
    "-DI32=" + std::to_string(i32),
    "-DI16=" + std::to_string(i16),
    "-DI8=" + std::to_string(i8),
    "-DI4=" + std::to_string(i4),
  };

  const std::vector<std::string> mathdx_include_dirs = mathdx::get_include_dirs();
  options.insert(options.begin(), mathdx_include_dirs.begin(), mathdx_include_dirs.end());

  if (firFilter) {
    options.push_back("-DNR_TAPS=" + std::to_string(firFilter->nrTaps));
    options.push_back("-DFIR_FILTER_SAMPLE_FORMAT=" + std::to_string(firFilter->sampleFormat));
  }

  if (applyDelays)
    options.push_back("-DAPPLY_DELAYS");

  if (bandPassCorrection)
    options.push_back("-DAPPLY_BANDPASS_WEIGHTS");

  if (output.scaleFactor != 1.0f)
    options.push_back("-DOUTPUT_SCALE_FACTOR=" + std::to_string(output.scaleFactor));

  if (input.customCode)
    options.push_back("-DINPUT_CUSTOM_CODE=" + *input.customCode);

  if (ringBufferSize)
    options.push_back("-DRING_BUFFER_SIZE=" + std::to_string(*ringBufferSize));

  //std::for_each(options.begin(), options.end(), [] (const std::string &e) { std::cout << e << ' '; }); std::cout << std::endl;

#if 0
  nvrtc::Program program("libfilter/FilterAndCorrect.cu");
#else
  // embed the CUDA source code in libfilter.so, so that it need not be installed separately
  // for runtime compilation
  // copy into std::string for '\0' termination
  std::string source(&_binary_libfilter_kernel_FilterAndCorrect_cu_start,
                     &_binary_libfilter_kernel_FilterAndCorrect_cu_end);
  nvrtc::Program program(source, "FilterAndCorrect.cu");
#endif

  try {
    program.compile(options);
    std::clog << program.getLog(); // print warnings
  } catch (nvrtc::Error &error) {
    std::cerr << program.getLog(); // print errors & warnings
    throw;
  }

  //std::ofstream cubin("out.ptx");
  //cubin << program.getPTX().data();
  return cu::Module((void *) program.getPTX().data());
}


Filter::Filter(const cu::Device &device, const FilterArgs &filterArgs)
:
  FilterArgs(filterArgs),
  nrChannelsPerThread(16),
  nrTimesPerIteration(16),
  filterModule(compileModule(device)),
  filterKernel(filterModule, "filterAndCorrect"),
  devFIRfilterWeights(firFilter ? std::optional<cu::DeviceMemory>(cu::DeviceMemory(nrChannels * firFilter->nrTaps * sizeof(float))) : std::nullopt),
  devBandPassWeights(bandPassCorrection ? std::optional<cu::DeviceMemory>(cu::DeviceMemory(nrChannels * sizeof(float))) : std::nullopt),
  _nrOperations([&] {
    uint64_t firFilterFLOPS = 2ULL * nrReceivers * (firFilter ? firFilter->nrTaps : 0) * nrChannels * nrSamplesPerChannel * nrPolarizations;
    uint64_t fftFLOPS       = 5ULL * nrReceivers * nrChannels * log2f(nrChannels) * nrSamplesPerChannel * nrPolarizations;
    uint64_t delaysFLOPS    = applyDelays ? 8ULL * nrReceivers * nrChannels * nrSamplesPerChannel * nrPolarizations : 0;
    uint64_t bandPassFLOPS  = bandPassCorrection ? 2ULL * nrReceivers * nrChannels * nrSamplesPerChannel * nrPolarizations : 0;

    return firFilterFLOPS + fftFLOPS + delaysFLOPS + bandPassFLOPS;
  } ())
{
  std::clog << "#registers = " << filterKernel.getAttribute(CU_FUNC_ATTRIBUTE_NUM_REGS ) << std::endl;
  //filterKernel.setCacheConfig(CU_FUNC_CACHE_PREFER_L1);

  cu::Stream stream;

  if (bandPassCorrection) {
    if (bandPassCorrection->weights.size() != nrChannels)
      throw std::runtime_error("must provide nrChannels bandpass weights");

    stream.memcpyHtoDAsync(*devBandPassWeights, bandPassCorrection->weights.data(), nrChannels * sizeof(float));
  }

  if (firFilter) {
    FilterBank filterBank(true, firFilter->nrTaps, nrChannels, KAISER);

    if (fft.shift)
      filterBank.negateWeights();

    float transposedWeights[firFilter->nrTaps][nrChannels];

    for (unsigned tap = 0; tap < firFilter->nrTaps; tap ++)
      for (unsigned channel = 0; channel < nrChannels; channel ++)
	transposedWeights[tap][channel] = filterBank.getWeights()[channel][tap];

    stream.memcpyHtoDAsync(*devFIRfilterWeights, transposedWeights, sizeof transposedWeights);
  }

  stream.synchronize();
}


void Filter::launchAsync(cu::Stream &stream, cu::DeviceMemory &devOutSamples, const cu::DeviceMemory &devInSamples, const std::optional<const cu::DeviceMemory> &devDelays, unsigned ringBufferStartIndex) // throw (cu::Error)
{
  std::vector<const void *> parameters = {
    devOutSamples.parameter(),
    devInSamples.parameter(),
    devFIRfilterWeights->parameter(),
  };

  if (devDelays != std::nullopt)
    parameters.push_back(devDelays->parameter());

  if (bandPassCorrection)
    parameters.push_back(devBandPassWeights->parameter());

  if (ringBufferSize)
    parameters.push_back(&ringBufferStartIndex);

  stream.launchKernel(filterKernel,
		      //nrPolarizations, nrReceivers, nrSamplesPerChannel / nrTimesPerIteration,
		      nrPolarizations * nrReceivers * nrSamplesPerChannel / nrTimesPerIteration, 1, 1,
		      (nrChannels + nrChannelsPerThread - 1) / nrChannelsPerThread, nrTimesPerIteration, 1,
		      0, parameters);
}


void Filter::launchAsync(CUstream stream, CUdeviceptr devOutSamples, CUdeviceptr devInSamples, std::optional<CUdeviceptr> devDelays, unsigned ringBufferStartIndex) // throw (cu::Error)
{
  if (devDelays != std::nullopt) {
    cu::Stream _stream(stream);
    cu::DeviceMemory _devOutSamples(devOutSamples), _devInSamples(devInSamples), _devDelays(*devDelays);
    launchAsync(_stream, _devOutSamples, _devInSamples, _devDelays);
  } else {
    launchAsync(cu::Stream(stream), cu::DeviceMemory(devOutSamples), cu::DeviceMemory(devInSamples), ringBufferStartIndex);
  }
}

}

