#include <cuda_fp16.h>
#include <math_constants.h>
//#include <math_functions.h>
//#include <cstdio>
#include <cufftdx.hpp>
#if defined __CUDA_ARCH__
#include <sm_61_intrinsics.hpp> // must include this explicitly for __dp4a when compiling with NVRTC
#endif


//#define SUBBAND_BANDWIDTH 195312.5

#define REAL	0
#define IMAG	1
#define COMPLEX	2


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


#if NR_BITS == 16
typedef short2  InputSample;
typedef __half2 OutputSample;
#elif NR_BITS == 8
typedef char2 InputSample, OutputSample;
#else
#error unsupport NR_BITS
#endif


template <typename T> __device__ inline __half2 sampleToComplexFloat(T sample)
{
  return make_half2(sample.x, sample.y);
}


#define NR_TIMES_PER_BLOCK      	(128 / (NR_BITS))
#define CHANNEL_INTEGRATION_FACTOR	(NR_CHANNELS_PER_SUBBAND == 1 ? 1 : (NR_CHANNELS_PER_SUBBAND - 1) / NR_OUTPUT_CHANNELS_PER_SUBBAND)
#define NR_OUTPUT_SAMPLES_PER_CHANNEL	(CHANNEL_INTEGRATION_FACTOR * NR_SAMPLES_PER_CHANNEL)


__device__ inline bool time_ok(unsigned time)
{
  return (NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1) * NR_CHANNELS_PER_SUBBAND % 64 == 0 || time < (NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1) * NR_CHANNELS_PER_SUBBAND;
}


__device__ inline bool recv_pol_ok(unsigned recv_pol)
{
  return NR_RECEIVERS * NR_POLARIZATIONS % 64 == 0 || recv_pol < NR_RECEIVERS * NR_POLARIZATIONS;
}


__device__ inline bool output_time_ok(unsigned time)
{
  return NR_SAMPLES_PER_CHANNEL % 64 == 0 || time < NR_SAMPLES_PER_CHANNEL;
}


__device__ inline bool output_channel_ok(unsigned channel)
{
  return NR_OUTPUT_CHANNELS_PER_SUBBAND * CHANNEL_INTEGRATION_FACTOR % 64 == 0 || channel < NR_OUTPUT_CHANNELS_PER_SUBBAND * CHANNEL_INTEGRATION_FACTOR;
}


extern "C" __global__ __launch_bounds__(32 * 32)
void transpose(
  InputSample output[NR_RECEIVERS * NR_POLARIZATIONS][(NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1) * NR_CHANNELS_PER_SUBBAND],
#if defined HAS_INTEGRATED_MEMORY && !defined TEST
  const InputSample input[NR_RING_BUFFER_SAMPLES_PER_SUBBAND][NR_RECEIVERS * NR_POLARIZATIONS],
  unsigned startIndex
#else
  const InputSample input[(NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1) * NR_CHANNELS_PER_SUBBAND][NR_RECEIVERS * NR_POLARIZATIONS]
#endif
)
{
  __shared__ InputSample tmp[64][64 + 1 /* one wider, to avoid bank conflicts */];

  unsigned recv_pol_major = blockIdx.x * 64;
  unsigned time_major     = blockIdx.y * 64;

  for (unsigned y = 0; y < 64; y += 32) {
    for (unsigned x = 0; x < 64; x += 32) {
      unsigned time     = time_major     + y + threadIdx.y;
      unsigned recv_pol = recv_pol_major + x + threadIdx.x;

      if (time_ok(time) && recv_pol_ok(recv_pol))
#if defined HAS_INTEGRATED_MEMORY && !defined TEST
	tmp[y + threadIdx.y][x + threadIdx.x] = input[(time + startIndex) % NR_RING_BUFFER_SAMPLES_PER_SUBBAND][recv_pol];
#else
	tmp[y + threadIdx.y][x + threadIdx.x] = input[time][recv_pol];
#endif
    }
  }

  __syncthreads();

  for (unsigned y = 0; y < 64; y += 32) {
    for (unsigned x = 0; x < 64; x += 32) {
      unsigned time     = time_major     + x + threadIdx.x;
      unsigned recv_pol = recv_pol_major + y + threadIdx.y;

      if (time_ok(time) && recv_pol_ok(recv_pol))
	output[recv_pol][time] = tmp[x + threadIdx.x][y + threadIdx.y];
    }
  }
}


extern "C" __global__ __launch_bounds__(NR_CHANNELS_PER_SUBBAND)
void filterAndCorrect(
  //OutputSample      output[NR_OUTPUT_CHANNELS_PER_SUBBAND][NR_OUTPUT_SAMPLES_PER_CHANNEL / NR_TIMES_PER_BLOCK][NR_RECEIVERS][NR_POLARIZATIONS][NR_TIMES_PER_BLOCK],
  OutputSample      output[NR_RECEIVERS][NR_POLARIZATIONS][NR_SAMPLES_PER_CHANNEL][NR_OUTPUT_CHANNELS_PER_SUBBAND * CHANNEL_INTEGRATION_FACTOR],
  const InputSample input[NR_RECEIVERS][NR_POLARIZATIONS][NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1][NR_CHANNELS_PER_SUBBAND],
  const float       filterWeights[NR_CHANNELS_PER_SUBBAND][NR_TAPS],
  const float       delays[NR_RECEIVERS][NR_POLARIZATIONS], // in seconds
  const float       bandPassWeights[NR_CHANNELS_PER_SUBBAND]
  //double	    subbandFrequency
)
{
#if defined __CUDA_ARCH__
  //unsigned infinity_count = 0, total_count = 0;

  //unsigned channel      = threadIdx.x;
  unsigned tid          = NR_CHANNELS_PER_SUBBAND / 16 * threadIdx.y + threadIdx.x;
  unsigned channel      = tid;
  unsigned polarization = blockIdx.x;
  unsigned receiver     = blockIdx.y;

  using namespace cufftdx;
  using FFT = decltype(Block() +
	      Size<NR_CHANNELS_PER_SUBBAND>() +
	      Type<fft_type::c2c>() +
	      Direction<fft_direction::forward>() +
	      Precision<float>() +
	      ElementsPerThread<16>() +
	      FFTsPerBlock<16>() +
	      //BlockDim<NR_CHANNELS_PER_SUBBAND, 1, 1>() + // Not yet supported by cuFFTDx
	      SM<__CUDA_ARCH__>());

  __shared__ FFT::value_type shared_mem[FFT::shared_memory_size / sizeof(FFT::value_type)];

#if NR_BITS == 16
  __half2 history[NR_TAPS], _weights[NR_TAPS];
#elif NR_BITS == 8
  union {
    signed char ch[NR_TAPS];
    char4       ch4[NR_TAPS / 4 + 1];
    int         i[NR_TAPS / 4 + 1];
  } history[COMPLEX], _weights;
#endif

  for (unsigned time = 0; time < NR_TAPS - 1; time ++) {
#if NR_BITS == 16
    history[time + 1] = sampleToComplexFloat(input[receiver][polarization][time][channel]);
#elif NR_BITS == 8
    history[REAL].ch[time + 1] = input[receiver][polarization][time][channel].x;
    history[IMAG].ch[time + 1] = input[receiver][polarization][time][channel].y;
#endif
  }

  for (unsigned tap = 0; tap < NR_TAPS; tap ++)
#if NR_BITS == 16
    //_weights[tap] = __float2half2_rn(filterWeights[channel][NR_TAPS - 1 - tap]);
    _weights[tap] = __float2half2_rn(filterWeights[channel][tap] / 16);
#elif NR_BITS == 
    //_weights.ch[tap] = roundf(128 * filterWeights[channel][NR_TAPS - 1 - tap]);
    _weights.ch[tap] = roundf(127 * filterWeights[channel][tap]);
#endif

  __shared__ float2 fft_data[16][NR_CHANNELS_PER_SUBBAND];

  //float  phi = -2 * CUDART_PI_F * delays[receiver][polarization];
  //float2 v   = make_float2(cosf(phi), sinf(phi));

  for (unsigned time_major = 0; time_major < NR_SAMPLES_PER_CHANNEL; time_major += 16) {
#pragma unroll
    for (unsigned time_minor = 0; time_minor < 16; time_minor ++) {
#if NR_BITS == 16
      history[time_minor] = sampleToComplexFloat(input[receiver][polarization][time_major + time_minor + NR_TAPS - 1][channel]);

      __half2 sum = make_half2(0, 0);

#pragma unroll
      for (unsigned tap = 0; tap < NR_TAPS; tap ++)
	sum = __hfma2(_weights[tap], history[(time_minor - tap) % NR_TAPS], sum);

      fft_data[time_minor][channel] = __half22float2(sum);
#elif NR_BITS == 8
      char2 sample = input[receiver][polarization][time_major + time_minor + NR_TAPS - 1][channel];
      history[REAL].ch[0] = sample.x;
      history[IMAG].ch[0] = sample.y;

      int sum[COMPLEX] = {0};

      for (unsigned ri = 0; ri < COMPLEX; ri ++) {
#if 0
	for (int tap = NR_TAPS / 4; -- tap >= 0;) {
	  sum[ri] = __dp4a(history[ri].i[tap], _weights.i[tap], sum[ri]);
	  history[ri].i[tap] = __funnelshift_l(history[ri].i[tap], history[ri].i[tap - 1], 8);
	}
#else
	for (unsigned tap = 0; tap < NR_TAPS; tap ++)
	  sum[ri] += (int) history[ri].ch[tap] * _weights.ch[tap];

	for (unsigned tap = NR_TAPS - 1; tap > 0; tap --)
	  history[ri].ch[tap] = history[ri].ch[tap - 1];
#endif
      }

      fft_data[time_minor][channel] = make_float2(sum[REAL], sum[IMAG]);
#endif
    }

    __syncthreads();

    FFT::value_type thread_data[FFT::storage_size];
    using complex_type = typename FFT::value_type;

    unsigned stride = size_of<FFT>::value / FFT::elements_per_thread;

    for (unsigned i = 0; i < FFT::elements_per_thread; ++ i)
      thread_data[i] = * ((FFT::value_type *) &fft_data[threadIdx.y][i * stride + threadIdx.x]);

    FFT().execute(thread_data, shared_mem);

    for (unsigned i = 0; i < FFT::elements_per_thread; ++ i)
      * ((complex_type *) &fft_data[threadIdx.y][i * stride + threadIdx.x]) = thread_data[i];

    __syncthreads();

    {
#if 0
      unsigned time_minor    = tid % 16U;
      unsigned channel_minor = tid / 16U;

      for (unsigned channel_major = 0; channel_major < NR_CHANNELS_PER_SUBBAND; channel_major += NR_CHANNELS_PER_SUBBAND / 16) {
	unsigned channel = channel_major + channel_minor;
	unsigned output_channel = (channel - 1) / CHANNEL_INTEGRATION_FACTOR;
	unsigned time = time_major + time_minor;
	unsigned output_time = time + ((channel - 1) % CHANNEL_INTEGRATION_FACTOR) * NR_SAMPLES_PER_CHANNEL;

	if (output_channel < NR_OUTPUT_CHANNELS_PER_SUBBAND) {
	  //double frequency = subbandFrequency - .5 * SUBBAND_BANDWIDTH + channel * (SUBBAND_BANDWIDTH / NR_CHANNELS_PER_SUBBAND);
	  float2 sample = bandPassWeights[channel] * fft_data[time_minor][channel]; // TODO: use __half2 ???
	  //sample = complexMul(sample, v);
//if ((float) sample.x != 0 || (float) sample.y != 0)
  //printf("GPU: output[%u][%u][%u][%u][%u] = (%f,%f)\n", output_channel, output_time / NR_TIMES_PER_BLOCK, receiver, polarization, output_time % NR_TIMES_PER_BLOCK, (float) sample.x, (float) sample.y);
#if NR_BITS == 16
	  //if (__hisinf(__habs(__float2half_rn(sample.x))))
            //++ infinity_count;
          //if (__hisinf(__habs(__float2half_rn(sample.y))))
            //++ infinity_count;
	  //total_count += 2;

	  output[output_channel][output_time / NR_TIMES_PER_BLOCK][receiver][polarization][output_time % NR_TIMES_PER_BLOCK] = __float22half2_rn(sample);
#elif NR_BITS == 8
	  output[output_channel][output_time / NR_TIMES_PER_BLOCK][receiver][polarization][output_time % NR_TIMES_PER_BLOCK] = make_char2(sample.x / 8192, sample.y / 8192);
#endif
	}
      }
#else
      for (unsigned time_minor = 0; time_minor < 16; time_minor ++) {
        unsigned time = time_major + time_minor;

        if (channel < NR_OUTPUT_CHANNELS_PER_SUBBAND * CHANNEL_INTEGRATION_FACTOR) {
          float2 sample = bandPassWeights[channel + 1] * fft_data[time_minor][channel + 1];
sample = make_float2(sample.x / 16, sample.y / 16);
	  //sample = complexMul(sample, v);
          output[receiver][polarization][time][channel] = __float22half2_rn(sample);
        }
      }
#endif
    }

    __syncthreads();
  }
#endif

  //if (infinity_count > 0)
    //printf("%u out of %u infinity\n", infinity_count, total_count);
}


extern "C" __global__ __launch_bounds__(32 * 32)
void postTranspose(
  OutputSample output[NR_OUTPUT_CHANNELS_PER_SUBBAND * CHANNEL_INTEGRATION_FACTOR][NR_SAMPLES_PER_CHANNEL / NR_TIMES_PER_BLOCK][NR_RECEIVERS * NR_POLARIZATIONS][NR_TIMES_PER_BLOCK],
  OutputSample input[NR_RECEIVERS * NR_POLARIZATIONS][NR_SAMPLES_PER_CHANNEL][NR_OUTPUT_CHANNELS_PER_SUBBAND * CHANNEL_INTEGRATION_FACTOR]
)
{
  __shared__ OutputSample tmp[64][65];

  unsigned recv_pol_major = blockIdx.x * (NR_TIMES_PER_BLOCK < 64 ? 64 / NR_TIMES_PER_BLOCK : 1);
  unsigned time_major     = blockIdx.y * (NR_TIMES_PER_BLOCK < 64 ? NR_TIMES_PER_BLOCK : 64);
  unsigned channel_major  = blockIdx.z * 64;

  for (unsigned y = 0; y < 64; y += 32) {
    for (unsigned x = 0; x < 64; x += 32) {
      unsigned time     = time_major     + (NR_TIMES_PER_BLOCK < 64 ? (y + threadIdx.y) % NR_TIMES_PER_BLOCK : y + threadIdx.y);
      unsigned recv_pol = recv_pol_major + (NR_TIMES_PER_BLOCK < 64 ? (y + threadIdx.y) / NR_TIMES_PER_BLOCK : 0);
      unsigned channel  = channel_major  + x + threadIdx.x;

      if (output_channel_ok(channel) && recv_pol_ok(recv_pol) && output_time_ok(time))
	tmp[y + threadIdx.y][x + threadIdx.x] = input[recv_pol][time][channel];
    }
  }

  __syncthreads();

  for (unsigned y = 0; y < 64; y += 32) {
    for (unsigned x = 0; x < 64; x += 32) {
      unsigned time     = time_major     + (NR_TIMES_PER_BLOCK < 64 ? (x + threadIdx.x) % NR_TIMES_PER_BLOCK : x + threadIdx.x);
      unsigned recv_pol = recv_pol_major + (NR_TIMES_PER_BLOCK < 64 ? (x + threadIdx.x) / NR_TIMES_PER_BLOCK : 0);
      unsigned channel  = channel_major  + y + threadIdx.y;

      if (output_channel_ok(channel) && recv_pol_ok(recv_pol) && output_time_ok(time))
	output[channel][time / NR_TIMES_PER_BLOCK][recv_pol][time % NR_TIMES_PER_BLOCK] = tmp[x + threadIdx.x][y + threadIdx.y];
    }
  }
}


#if defined TEST
// FIXME
// nvcc -DTEST -DNR_RECEIVERS=2 -DNR_BITS=16 -DNR_CHANNELS_PER_SUBBAND=64 -DNR_OUTPUT_CHANNELS_PER_SUBBAND=63 -DNR_SAMPLES_PER_CHANNEL=32 -DNR_POLARIZATIONS=2 -DNR_TAPS=16 -std=c++17 -arch=sm_87 -I/home/romein/packages/nvidia-mathdx-22.11.0-Linux/nvidia/mathdx/22.11/include -I. Correlator/Kernels/FilterAndCorrect.cu
		      //(ps.nrStations() * ps.nrPolarizations() + nrRecvPolPerBlock - 1) / nrRecvPolPerBlock, 1, (ps.nrChannelsPerSubband() + 31) / 32,

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
  InputSample  (*input)[NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1][NR_CHANNELS_PER_SUBBAND][NR_RECEIVERS][NR_POLARIZATIONS];
  InputSample  (*transposedInput)[NR_RECEIVERS][NR_POLARIZATIONS][NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1][NR_CHANNELS_PER_SUBBAND];
  OutputSample (*output)[NR_CHANNELS_PER_SUBBAND][NR_SAMPLES_PER_CHANNEL / NR_TIMES_PER_BLOCK][NR_RECEIVERS][NR_POLARIZATIONS][NR_TIMES_PER_BLOCK];
  float        (*filterWeights)[NR_CHANNELS_PER_SUBBAND][NR_TAPS];
  float        (*delays)[NR_RECEIVERS][NR_POLARIZATIONS]; // in seconds
  float        (*bandPassWeights)[NR_CHANNELS_PER_SUBBAND];

  checkCudaCall(cudaMallocManaged(&output, sizeof(*output)));
  checkCudaCall(cudaMallocManaged(&input, sizeof(*input)));
  checkCudaCall(cudaMallocManaged(&transposedInput, sizeof(*transposedInput)));
  checkCudaCall(cudaMallocManaged(&filterWeights, sizeof(*filterWeights)));
  checkCudaCall(cudaMallocManaged(&delays, sizeof(*delays)));
  checkCudaCall(cudaMallocManaged(&bandPassWeights, sizeof(*bandPassWeights)));

#if 0
  (*filterWeights)[0][15] = 1.0f;
#elif 1
for (unsigned channel = 0; channel < NR_CHANNELS_PER_SUBBAND; channel ++)
  (*filterWeights)[channel][0] = 1;
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
  for (unsigned time = 0; time < NR_SAMPLES_PER_CHANNEL * NR_CHANNELS_PER_SUBBAND; time ++) {
    float phi = 2 * CUDART_PI_F * time * 6 / NR_SAMPLES_PER_CHANNEL;
#if NR_BITS == 16
    (*input)[time / NR_CHANNELS_PER_SUBBAND][time % NR_CHANNELS_PER_SUBBAND][1][0] = make_short2((short) (128 * cos(phi)), (short) (128 * sin(phi)));
#elif NR_BITS == 8
    (*input)[time / NR_CHANNELS_PER_SUBBAND][time % NR_CHANNELS_PER_SUBBAND][1][0] = make_char2((short) (127 * cos(phi)), (short) (127 * sin(phi)));
#endif
  }
#else
  for (unsigned channel = 0; channel < NR_CHANNELS_PER_SUBBAND; channel ++) {
    float phi = 2 * CUDART_PI_F * 5 * channel / NR_CHANNELS_PER_SUBBAND;
#if NR_BITS == 16
    (*input)[22][channel][1][0] = make_short2((short) roundf(32 * cos(phi)), (short) roundf(32 * sin(phi)));
#elif NR_BITS == 8
    (*input)[22][channel][1][0] = make_char2((short) roundf(32 * cos(phi)), (short) roundf(32 * sin(phi)));
#endif
  }
#endif

  transpose<<<
    dim3((((NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1) * NR_CHANNELS_PER_SUBBAND) + 31) / 32, (NR_RECEIVERS * NR_POLARIZATIONS + 31) / 32),
    dim3(32, 32)
  >>>(
    * (InputSample (*)[NR_RECEIVERS * NR_POLARIZATIONS][(NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1) * NR_CHANNELS_PER_SUBBAND]) transposedInput,
    * (InputSample (*)[(NR_SAMPLES_PER_CHANNEL + NR_TAPS - 1) * NR_CHANNELS_PER_SUBBAND][NR_RECEIVERS * NR_POLARIZATIONS]) input
  );

  filterAndCorrect<<<
    dim3(NR_POLARIZATIONS, NR_RECEIVERS),
    dim3(NR_CHANNELS_PER_SUBBAND / 16, 16)
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
	for (unsigned channel = 0; channel < NR_CHANNELS_PER_SUBBAND; channel ++) {
	  OutputSample &sample = (*output)[channel][time / NR_TIMES_PER_BLOCK][receiver][polarization][time % NR_TIMES_PER_BLOCK];

	  if ((int) sample.x != 0 || (int) sample.y != 0)
	    std::cout << "output[" << channel << "][" << time / NR_TIMES_PER_BLOCK << "][" << receiver << "][" << polarization << "][" << time % NR_TIMES_PER_BLOCK << "] = " << sample << std::endl;
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
