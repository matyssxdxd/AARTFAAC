#include <cuda_fp16.h>



#define REAL	0
#define IMAG	1
#define COMPLEX	2


#if NR_BITS == 16
typedef short2  InputSample;
typedef __half2 OutputSample;
#elif NR_BITS == 8
typedef char2 InputSample, OutputSample;
#else
#error unsupport NR_BITS
#endif


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
