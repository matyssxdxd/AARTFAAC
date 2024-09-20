#include "math.cl"

#if NR_CHANNELS == 1
#error FIXME
#undef BANDPASS_CORRECTION
#endif


typedef __global fcomplex OutputDataType[NR_CHANNELS][NR_SAMPLES_PER_CHANNEL][NR_STATIONS * NR_POLARIZATIONS];
#if NR_CHANNELS == 1
#if NR_BITS_PER_SAMPLE == 16
typedef __global short_complex InputDataType[NR_STATIONS * NR_POLARIZATIONS][NR_SAMPLES_PER_SUBBAND];
#elif NR_BITS_PER_SAMPLE == 8
typedef __global char_complex InputDataType[NR_STATIONS * NR_POLARIZATIONS][NR_SAMPLES_PER_SUBBAND];
#else
#error unsupport NR_BITS_PER_SAMPLE
#endif
#else
typedef __global fcomplex InputDataType[NR_SAMPLES_PER_CHANNEL][NR_STATIONS * NR_POLARIZATIONS][NR_CHANNELS];
#endif
typedef __global const float DelaysType[NR_BEAMS][NR_STATIONS * NR_POLARIZATIONS]; // in seconds
typedef __global const float PhaseOffsetsType[NR_STATIONS * NR_POLARIZATIONS]; // in radians
typedef __constant const float BandPassWeightsType[NR_CHANNELS];



__kernel __attribute__((reqd_work_group_size(16, 16, 1)))
void applyDelaysAndCorrectBandPass(OutputDataType outputData,
                                   const InputDataType inputData,
                                   float subbandFrequency,
                                   unsigned beam,
				   const DelaysType delaysAtBegin,
				   const DelaysType delaysAfterEnd,
				   //const PhaseOffsetsType phaseOffsets,
				   const BandPassWeightsType bandPassWeights)
{
#if NR_CHANNELS > 1
  __local fcomplex tmp[16][17];

  uint minor = get_local_id(0);
  uint major = get_local_id(1);
  uint station_pol = 16 * get_group_id(0);//get_global_id(0) & ~(16 - 1);
  uint channel = 16 * get_group_id(1);//get_global_id(1) & ~(16 - 1);
#else
  uint station = get_global_id(0);
#endif

#if defined DELAY_COMPENSATION
#if NR_CHANNELS == 1
#else
  float frequency = subbandFrequency - .5f * SUBBAND_BANDWIDTH + (channel + minor) * (SUBBAND_BANDWIDTH / NR_CHANNELS);
  float delayAtBegin = delaysAtBegin[beam][station_pol + major];
  float delayAfterEnd = delaysAfterEnd[beam][station_pol + major];
#endif
  float phiBegin = -2 * 3.1415926535f * delayAtBegin;
  float phiEnd = -2 * 3.1415926535f * delayAfterEnd;
  float deltaPhi = (phiEnd - phiBegin) / NR_SAMPLES_PER_CHANNEL;
#if NR_CHANNELS == 1
#else
  float myPhiBegin = (phiBegin /* + startTime * deltaPhi */) * frequency;// + phaseOffsets[FIXME: station_pol + major];
  float myPhiDelta = deltaPhi * frequency;
#endif
  fcomplex v = cexp(myPhiBegin);
  fcomplex dv = cexp(myPhiDelta);
#endif

#if defined BANDPASS_CORRECTION
  float weight = bandPassWeights[channel + minor];
#endif

#if defined DELAY_COMPENSATION && defined BANDPASS_CORRECTION
  v *= weight;
#endif

#pragma unroll 1 // strange, compiler seems to multiply the unroll factor by 8
  for (uint time = 0; time < NR_SAMPLES_PER_CHANNEL; time ++) {
    fcomplex sample;

    if (NR_STATIONS * NR_POLARIZATIONS % 16 == 0 || station_pol + major < NR_STATIONS * NR_POLARIZATIONS)
      sample = inputData[time][station_pol + major][channel + minor];

#if defined DELAY_COMPENSATION
    sample = cmul(sample, v);
    v = cmul(v, dv);
#elif defined BANDPASS_CORRECTION
    sample *= weight;
#endif

#if NR_CHANNELS == 1
#else
    if (NR_STATIONS * NR_POLARIZATIONS % 16 == 0 || station_pol + major < NR_STATIONS * NR_POLARIZATIONS)
      tmp[minor][major] = sample;

    barrier(CLK_LOCAL_MEM_FENCE);

    if (NR_STATIONS * NR_POLARIZATIONS % 16 == 0 || station_pol + minor < NR_STATIONS * NR_POLARIZATIONS)
      outputData[channel + major][time][station_pol + minor] = tmp[major][minor];

    barrier(CLK_LOCAL_MEM_FENCE);
#endif
  }
}
