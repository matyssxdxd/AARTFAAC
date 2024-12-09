#include <cuda_fp8.h>
#include <cuda_fp16.h>
#include <math_constants.h>
#include <cufftdx.hpp>

#if defined __CUDA_ARCH__
#include <sm_61_intrinsics.hpp> // must include this explicitly for __dp4a when compiling with NVRTC
#endif


//#define SUBBAND_BANDWIDTH 195312.5

#define REAL	0
#define IMAG	1
#define COMPLEX	2

#define ALIGN(N, A) (((N) + (A) - 1) / (A) * (A))
#define MIN(A,B) ((A)<(B)?(A):(B))

inline __device__ void prefetch(const void *ptr, unsigned size, unsigned tid, unsigned nr_threads)
{
#if __CUDA_ARCH__ >= 900
  if (tid == 0)
    asm ("cp.async.bulk.prefetch.L2.global [%0],%1;" :: "l" (ptr), "r" (size));
#else
  for (unsigned i = 0; i < size; i += 8 * nr_threads)
    asm ("prefetch.global.L2 [%0];" :: "l" (static_cast<const char *>(ptr) + i + 8 * tid));
#endif
}


typedef float2 fcomplex; // std::complex not properly supported yet

__device__ inline fcomplex operator + (fcomplex a, fcomplex b)
{
  return make_float2(a.x + b.x, a.y + b.y);
}

__device__ inline fcomplex operator - (fcomplex a, fcomplex b)
{
  return make_float2(a.x - b.x, a.y - b.y);
}

__device__ inline fcomplex operator * (float a, fcomplex b)
{
  return make_float2(a * b.x, a * b.y);
}

__device__ inline fcomplex operator *= (fcomplex &a, float b)
{
  return make_float2(a.x *= b, a.y *= b);
}

#if 0
__device__ inline __half2 operator * (__half2 a, __half2 b)
{
  return make_half2(a.x * b.x - a.y * b.y, a.x * b.y - a.y * b.x); // TODO: use vector intrinsics
}
#endif

__device__ inline fcomplex operator += (fcomplex &a, fcomplex b)
{
  return make_float2(a.x += b.x, a.y += b.y);
}


#define complexMul(a,b) make_float2((a).x * (b).x - (a).y * (b).y, (a).y * (b).x + (a).x * (b).y)


#if INPUT_SAMPLE_FORMAT == I16
typedef short2  InputSample;
#elif INPUT_SAMPLE_FORMAT == I8
typedef char2 InputSample;
#else
#error unsupport input sample format
#endif

#if OUTPUT_SAMPLE_FORMAT == FP16
typedef __half2 OutputSample;
#elif OUTPUT_SAMPLE_FORMAT == E4M3
typedef __nv_fp8x2_e4m3 OutputSample;
#elif OUTPUT_SAMPLE_FORMAT == E5M2
typedef __nv_fp8x2_e5m2 OutputSample;
#elif OUTPUT_SAMPLE_FORMAT == I8
typedef char2 OutputSample;
#else
#error unsupported output sample format
#endif


#if FIR_FILTER_SAMPLE_FORMAT == FP32

template <typename T> __device__ inline float2 sampleToComplexFloat(T sample)
{
  return make_float2(sample.x, sample.y);
}

#elif FIR_FILTER_SAMPLE_FORMAT == FP16

template <typename T> __device__ inline __half2 sampleToComplexFloat(T sample)
{
  return make_half2(sample.x, sample.y);
}

#endif

#define NR_BITS(FORMAT) (FORMAT == FP32 || FORMAT == I32 ? 32 : \
			 FORMAT == FP16 || FORMAT == BF16 || FORMAT == I16 ? 16 : \
			 FORMAT == E4M3 || FORMAT == E5M2 || FORMAT ==  I8 ?  8 : \
			 FORMAT == I4 ? 4 : 0)
#define NR_TIMES_PER_OUTPUT_BLOCK      	(128 / NR_BITS(OUTPUT_SAMPLE_FORMAT))


#if defined INPUT_CUSTOM_CODE
INPUT_CUSTOM_CODE
#elif defined RING_BUFFER_SIZE

typedef InputSample InputType[NR_RECEIVERS][NR_POLARIZATIONS][RING_BUFFER_SIZE];

InputSample readSample(const InputType input, unsigned receiver, unsigned polarization, unsigned time, unsigned channel, unsigned ringBufferStartIndex)
{
  return input[receiver][polarization][(time * NR_CHANNELS + channel + ringBufferStartIndex) % RING_BUFFER_SIZE];
}

#else

typedef InputSample InputType[NR_RECEIVERS][NR_POLARIZATIONS][NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1][NR_CHANNELS];

//InputSample readSample(InputType input, unsigned receiver, unsigned polarization, unsigned time, unsigned channel)
//{
//  return input[receiver][polarization][time][channel];
//}

#endif


#define NR_THREADS ((NR_CHANNELS + NR_CHANNELS_PER_THREAD - 1) / NR_CHANNELS_PER_THREAD * NR_TIMES_PER_ITERATION)

extern "C" __global__ __launch_bounds__(NR_THREADS /*, 3 */)
void filterAndCorrect(
  OutputSample      output[NR_CHANNELS][NR_SAMPLES_PER_CHANNEL / NR_TIMES_PER_OUTPUT_BLOCK][NR_RECEIVERS][NR_POLARIZATIONS][NR_TIMES_PER_OUTPUT_BLOCK],
  const InputType   input,
  const float       filterWeights[NR_TAPS][NR_CHANNELS]
#if defined APPLY_DELAYS
  , const float     delays[NR_RECEIVERS][NR_POLARIZATIONS] // in seconds
#endif
#if defined APPLY_BANDPASS_WEIGHTS
  , const float     bandPassWeights[NR_CHANNELS]
#endif
#if defined RING_BUFFER_SIZE
  , unsigned        ringBufferStartIndex
#endif
  //double	    subbandFrequency
)
{
#if defined __CUDA_ARCH__
  unsigned tid = threadIdx.y * blockDim.x + threadIdx.x;

#if 0
  unsigned polarization = blockIdx.x;
  unsigned receiver     = blockIdx.y;
  unsigned time_major   = blockIdx.z * NR_TIMES_PER_ITERATION;
#else
#if __CUDA_ARCH__ == 870
  constexpr unsigned divisor = 1;
#else
  constexpr unsigned divisor = MIN(8, NR_SAMPLES_PER_CHANNEL / NR_TIMES_PER_ITERATION);
#endif

  unsigned polarization = (blockIdx.x / divisor) % NR_POLARIZATIONS;
  unsigned receiver     = (blockIdx.x / (divisor * NR_POLARIZATIONS)) % NR_RECEIVERS;
  unsigned time_major   = (blockIdx.x % divisor + (blockIdx.x / (divisor * NR_POLARIZATIONS * NR_RECEIVERS)) * divisor) * NR_TIMES_PER_ITERATION;
#endif

  //prefetch(input[receiver][polarization][time_major], NR_CHANNELS * (NR_TAPS - 1 + NR_TIMES_PER_ITERATION) * sizeof(InputSample), tid, NR_THREADS);

#if FFT_SAMPLE_FORMAT != FP32
#error unspported FFT precision
#endif

  using namespace cufftdx;
  using FFT = decltype(Block() +
	      Size<NR_CHANNELS>() +
	      Type<fft_type::c2c>() +
	      Direction<fft_direction::forward>() +
	      Precision<float>() +
	      ElementsPerThread<NR_CHANNELS_PER_THREAD>() +
	      FFTsPerBlock<NR_TIMES_PER_ITERATION>() +
	      //BlockDim<NR_CHANNELS, 1, 1>() + // Not yet supported by cuFFTDx
	      SM<__CUDA_ARCH__>());

  __shared__ union {
    FFT::value_type shared_mem[FFT::shared_memory_size / sizeof(FFT::value_type)];
    float2 fft_data[NR_TIMES_PER_ITERATION][NR_CHANNELS | 1];
  } u;

#if defined APPLY_DELAYS
  float  phi = -2 * CUDART_PI_F * delays[receiver][polarization];
  float2 v   = make_float2(cosf(phi), sinf(phi));
#endif

  FFT::value_type thread_data[FFT::storage_size];
  using complex_type = typename FFT::value_type;

#pragma unroll
  for (unsigned channel_major = 0, channel; (channel = channel_major + tid) < NR_CHANNELS; channel_major += NR_THREADS) {
    float2 cachedSamples[NR_TIMES_PER_ITERATION + NR_TAPS - 1];

    for (unsigned i = 0; i < NR_TIMES_PER_ITERATION + NR_TAPS - 1; i ++)
#if defined INPUT_CUSTOM_CODE
      cachedSamples[i] = sampleToComplexFloat(readSample(input, receiver, polarization, time_major + i, channel));
#elif RING_BUFFER_SIZE
      cachedSamples[i] = sampleToComplexFloat(readSample(input, receiver, polarization, time_major + i, channel, ringBufferStartIndex));
#else
      cachedSamples[i] = sampleToComplexFloat(input[receiver][polarization][time_major + i][channel]);
#endif

#pragma unroll
    for (unsigned time_minor = 0; time_minor < NR_TIMES_PER_ITERATION; time_minor ++)  {
#if FIR_FILTER_SAMPLE_FORMAT == FP32
      float2 sum = make_float2(0, 0);

#pragma unroll
      for (unsigned tap = 0; tap < NR_TAPS; tap ++)
	sum += filterWeights[NR_TAPS - 1 - tap][channel] * cachedSamples[time_minor + tap];

      u.fft_data[time_minor][channel] = sum;
      //thread_data[channel_idx] = FFT::value_type(sum.x, sum.y);
#elif FIR_FILTER_SAMPLE_FORMAT == FP16
      __half2 sum = make_half2(0, 0);

#pragma unroll
      for (unsigned tap = 0; tap < NR_TAPS; tap ++)
	sum = __hfma2(__float2half2_rn(filterWeights[NR_TAPS - 1 - tap][channel] /* / 16 */), sampleToComplexFloat(input[receiver][polarization][time_major + time_minor + tap][channel]), sum);

      u.fft_data[time_minor][channel] = __half22float2(sum);
#elif FIR_FILTER_SAMPLE_FORMAT == I8
      int sum[COMPLEX] = {0};

	for (unsigned ri = 0; ri < COMPLEX; ri ++) {
#if 0
	for (int tap = NR_TAPS / 4; -- tap >= 0;) {
	  sum[ri] = __dp4a(history[ri].i[tap], _weights.i[tap], sum[ri]);
	  history[ri].i[tap] = __funnelshift_l(history[ri].i[tap], history[ri].i[tap - 1], 8);
	}
#else
	for (unsigned tap = 0; tap < NR_TAPS; tap ++)
	  sum[ri] += (int) history[ri].ch[tap] * roundf(127 * filterWeights[tap][channel]);

	for (unsigned tap = NR_TAPS - 1; tap > 0; tap --)
	  history[ri].ch[tap] = history[ri].ch[tap - 1];
#endif
      }

      u.fft_data[time_minor][channel] = make_float2(sum[REAL], sum[IMAG]);
#endif
    }
  }

  __syncthreads();

  unsigned stride = size_of<FFT>::value / FFT::elements_per_thread;

  for (unsigned i = 0; i < FFT::elements_per_thread; ++ i)
    thread_data[i] = * ((FFT::value_type *) &u.fft_data[threadIdx.y][i * stride + threadIdx.x]);

  FFT().execute(thread_data, u.shared_mem);

  for (unsigned i = 0; i < FFT::elements_per_thread; ++ i)
    * ((complex_type *) &u.fft_data[threadIdx.y][i * stride + threadIdx.x]) = thread_data[i];

  __syncthreads();

  {
    for (unsigned i = 0; i < NR_CHANNELS * NR_TIMES_PER_ITERATION; i += NR_THREADS) {
      unsigned channel    = (i + tid) / NR_TIMES_PER_ITERATION;
      unsigned time_minor = (i + tid) % NR_TIMES_PER_ITERATION;
      unsigned time       = time_major + time_minor;

      /*if (NR_CHANNELS % 16 == 0 || channel < NR_CHANNELS)*/ {
	//double frequency = subbandFrequency - .5 * SUBBAND_BANDWIDTH + channel * (SUBBAND_BANDWIDTH / NR_CHANNELS);
	float2 sample = u.fft_data[time_minor][channel]; // TODO: use __half2 ???
#if defined APPLY_BANDPASS_WEIGHTS
	sample *= bandPassWeights[channel];
#endif
#if defined APPLY_DELAYS
	sample = complexMul(sample, v);
#endif
#if defined OUTPUT_SCALE_FACTOR
	sample *= OUTPUT_SCALE_FACTOR;
#endif
#if OUTPUT_SAMPLE_FORMAT == FP16
	OutputSample outputSample = __float22half2_rn(sample);
#elif OUTPUT_SAMPLE_FORMAT == E4M3
	OutputSample outputSample = __nv_fp8x2_e4m3(sample);
#elif OUTPUT_SAMPLE_FORMAT == E5M2
	OutputSample outputSample = __nv_fp8x2_e5m2(sample);
#elif OUTPUT_SAMPLE_FORMAT == I8
	sample = make_float2(fmaxf(sample.x, -127.f), fmaxf(sample.y, -127.f)); // TCC cannot handle -128 as a calue
	sample = make_float2(fminf(sample.x,  127.f), fminf(sample.y,  127.f));
	sample = make_float2(rintf(sample.x), rintf(sample.y));

	OutputSample outputSample = make_char2(sample.x, sample.y);
#endif
	output[channel][time / NR_TIMES_PER_OUTPUT_BLOCK][receiver][polarization][time % NR_TIMES_PER_OUTPUT_BLOCK] = outputSample;
      }
    }
  }
#endif
}


#if defined TEST
#warning FIXME
// nvcc -DTEST -DNR_RECEIVERS=2 -DNR_BITS=16 -DNR_CHANNELS=64 -DNR_SAMPLES_PER_CHANNEL=32 -DNR_POLARIZATIONS=2 -DNR_TAPS=16 -std=c++17 -arch=sm_87 -I/home/romein/packages/nvidia-mathdx-22.11.0-Linux/nvidia/mathdx/22.11/include -I. Correlator/Kernels/FilterAndCorrect.cu

#include <iostream>

std::ostream &operator << (std::ostream &str, OutputSample sample)
{
  return str << '(' << (float) sample.x << ", " << (float) sample.y << ')';
}


inline void checkCudaCall(cudaError_t error)
{
  if (error != cudaSuccess) {
    std::cerr << "error " << error << std::endl;
    exit(1);
  }
}


#if 0
inline __host__ bool operator != (const OutputSample &a, const OutputSample &b)
{
  return a.x != b.x || a.y != b.y;
}
#endif


static const float constWeights[] = {
//#include "../../weights.txt"
#include "weights.txt"
};

#include <string>
#include <stdexcept>

int main()
{
  InputSample  (*input)[NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1][NR_CHANNELS][NR_RECEIVERS][NR_POLARIZATIONS];
  InputSample  (*transposedInput)[NR_RECEIVERS][NR_POLARIZATIONS][NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1][NR_CHANNELS];
  OutputSample (*output)[NR_CHANNELS][NR_SAMPLES_PER_CHANNEL / NR_TIMES_PER_OUTPUT_BLOCK][NR_RECEIVERS][NR_POLARIZATIONS][NR_TIMES_PER_OUTPUT_BLOCK];
  float        (*filterWeights)[NR_TAPS][NR_CHANNELS];
  float        (*delays)[NR_RECEIVERS][NR_POLARIZATIONS]; // in seconds
  float        (*bandPassWeights)[NR_CHANNELS];

  checkCudaCall(cudaMallocManaged(&output, sizeof(*output)));
  checkCudaCall(cudaMallocManaged(&input, sizeof(*input)));
  checkCudaCall(cudaMallocManaged(&transposedInput, sizeof(*transposedInput)));
  checkCudaCall(cudaMallocManaged(&filterWeights, sizeof(*filterWeights)));
  checkCudaCall(cudaMallocManaged(&delays, sizeof(*delays)));
  checkCudaCall(cudaMallocManaged(&bandPassWeights, sizeof(*bandPassWeights)));

#if 0
  (*filterWeights)[15][0] = 1.0f;
#elif 1
for (unsigned channel = 0; channel < NR_CHANNELS; channel ++)
  (*filterWeights)[0][channel] = 1;
#else
  memcpy(filterWeights, constWeights, sizeof constWeights);
#endif

  for (float &bandPassWeight : (*bandPassWeights))
    bandPassWeight = 1.0f;

#if 0
#if NR_BITS == 16
  (*input)[22][4][1][0] = make_short2(128, 0);
#elif NR_BITS == 8
  (*input)[22][4][1][0] = make_char2(127, 0);
#endif
#elif 0
  for (unsigned time = 0; time < NR_SAMPLES_PER_CHANNEL * NR_CHANNELS; time ++) {
    float phi = 2 * CUDART_PI_F * time * 6 / NR_SAMPLES_PER_CHANNEL;
#if NR_BITS == 16
    (*input)[time / NR_CHANNELS][time % NR_CHANNELS][1][0] = make_short2((short) (128 * cos(phi)), (short) (128 * sin(phi)));
#elif NR_BITS == 8
    (*input)[time / NR_CHANNELS][time % NR_CHANNELS][1][0] = make_char2((short) (127 * cos(phi)), (short) (127 * sin(phi)));
#endif
  }
#else
  for (unsigned channel = 0; channel < NR_CHANNELS; channel ++) {
    float phi = 2 * CUDART_PI_F * 5 * channel / NR_CHANNELS;
#if NR_BITS == 16
    (*input)[22][channel][1][0] = make_short2((short) roundf(32 * cos(phi)), (short) roundf(32 * sin(phi)));
#elif NR_BITS == 8
    (*input)[22][channel][1][0] = make_char2((short) roundf(32 * cos(phi)), (short) roundf(32 * sin(phi)));
#endif
  }
#endif

  transpose<<<
    dim3((((NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1) * NR_CHANNELS) + 31) / 32, (NR_RECEIVERS * NR_POLARIZATIONS + 31) / 32),
    dim3(32, 32)
  >>>(
    * (InputSample (*)[NR_RECEIVERS * NR_POLARIZATIONS][(NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1) * NR_CHANNELS]) transposedInput,
    * (InputSample (*)[(NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1) * NR_CHANNELS][NR_RECEIVERS * NR_POLARIZATIONS]) input
  );

  filterAndCorrect<<<
    dim3(NR_POLARIZATIONS, NR_RECEIVERS),
    dim3(NR_CHANNELS / 16, 16)
  >>>(
    *output,
    *transposedInput,
    *filterWeights,
    *delays,
    *bandPassWeights
    /*, 60e6f*/
  );

  checkCudaCall(cudaDeviceSynchronize());

#if 1
  std::cout << "testing ..." << std::endl;

  for (unsigned receiver = 0; receiver < NR_RECEIVERS; receiver ++)
    for (unsigned polarization = 0; polarization < NR_POLARIZATIONS; polarization ++)
      for (unsigned time = 0; time < NR_SAMPLES_PER_CHANNEL; time ++)
	for (unsigned channel = 0; channel < NR_CHANNELS; channel ++) {
	  OutputSample &sample = (*output)[channel][time / NR_TIMES_PER_OUTPUT_BLOCK][receiver][polarization][time % NR_TIMES_PER_OUTPUT_BLOCK];

	  if ((int) sample.x != 0 || (int) sample.y != 0)
	    std::cout << "output[" << channel << "][" << time / NR_TIMES_PER_OUTPUT_BLOCK << "][" << receiver << "][" << polarization << "][" << time % NR_TIMES_PER_OUTPUT_BLOCK << "] = " << sample << std::endl;
	}
#endif

  checkCudaCall(cudaFree(bandPassWeights));
  checkCudaCall(cudaFree(delays));
  checkCudaCall(cudaFree(filterWeights));
  checkCudaCall(cudaFree(transposedInput));
  checkCudaCall(cudaFree(input));
  checkCudaCall(cudaFree(output));
}
#endif
