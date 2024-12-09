#if !defined FILTER_H
#define FILTER_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cudawrappers/cu.hpp>


namespace tcc
{
  struct FilterArgs {
    enum Format { fp32, fp16, bf16, e4m3, e5m2, i32, i16, i8, i4 };

    static size_t nrBits(enum Format format) { // TODO: this should be a member function of some Format class
      return format == fp32 || format == i32 ? 32 :
	     format == fp16 || format == bf16 || format == i16 ? 16 :
	     format == e4m3 || format == e5m2 || format == i8 ? 8 :
	     format == i4 ? 4 : 0;
    }

    unsigned nrReceivers         = 1;
    unsigned nrChannels          = 256;
    unsigned nrSamplesPerChannel = 768;
    unsigned nrPolarizations     = 2;

    struct Input {
      Format                     sampleFormat = i16;
      std::optional<std::string> customCode  = std::nullopt;
    } input;

    struct FIR_Filter {
      unsigned nrTaps       = 16;
      Format   sampleFormat = fp32;
    };
    std::optional<FIR_Filter> firFilter = FIR_Filter(); // FIXME: not providing this "option" is not yet supported

    struct FFT {
      Format sampleFormat = fp32;
      bool   shift        = true;
    } fft;

    bool applyDelays = false;

    struct BandPassCorrection {
      std::vector<float> weights;
    };
    std::optional<BandPassCorrection> bandPassCorrection = std::nullopt;

    std::optional<unsigned>           ringBufferSize  = std::nullopt;

    struct Output {
      Format sampleFormat = fp16;
      float  scaleFactor  = 1.0f;
    } output;
  };

  class Filter : FilterArgs
  {
    public:
      Filter(const cu::Device &, const FilterArgs &);
      void launchAsync(cu::Stream &, cu::DeviceMemory &devOutSamples, const cu::DeviceMemory &devInSamples, const std::optional<const cu::DeviceMemory> &devDelays = std::nullopt, unsigned ringBufferStartIndex = 0); // throw (cu::Error)
      void launchAsync(CUstream, CUdeviceptr outSamples, CUdeviceptr inSamples, std::optional<CUdeviceptr> devDelays = std::nullopt, unsigned ringBufferStartIndex = 0); // throw (cu::Error)

      uint64_t nrOperations() const
      {
	return _nrOperations;
      }

    private:
      std::string      findNVRTCincludePath() const;

      cu::Module       compileModule(const cu::Device &);

      const unsigned   nrChannelsPerThread;
      const unsigned   nrTimesPerIteration;

      cu::Module       filterModule;
      cu::Function     filterKernel;
      std::optional<cu::DeviceMemory> devFIRfilterWeights;
      std::optional<cu::DeviceMemory> devBandPassWeights;
      const uint64_t   _nrOperations;
  };
}

#endif

