#include "math.cl"

#define NR_STATIONS_PER_BLOCK   32
#define NR_TIMES_PER_BLOCK      8

#define CHANNEL_INTEGRATION_FACTOR ((NR_CHANNELS - 1) / NR_OUTPUT_CHANNELS)

#define NR_BASELINES            (NR_STATIONS * (NR_STATIONS + 1) / 2)

#if CORRELATION_MODE == 0xF
typedef fcomplex4 VisType;
#elif CORRELATION_MODE == 0x9
typedef fcomplex2 VisType;
#elif CORRELATION_MODE == 0x1 || CORRELATION_MODE == 0x8
typedef fcomplex  VisType;
#endif

typedef __global fcomplex2 CorrectedDataType[NR_CHANNELS][NR_SAMPLES_PER_CHANNEL][NR_STATIONS];
typedef __global VisType VisibilitiesType[NR_BASELINES][NR_OUTPUT_CHANNELS];


__kernel
__attribute__((vec_type_hint(float)))
void correlateTriangleKernel(VisibilitiesType visibilities,
                             const CorrectedDataType correctedData)
{
  __local fcomplex samples[2][NR_POLARIZATIONS][NR_TIMES_PER_BLOCK][NR_STATIONS_PER_BLOCK / 2];

  uint block = get_global_id(1);

  uint firstStation = block * NR_STATIONS_PER_BLOCK;

  uint statXoffset = get_local_id(0) / (NR_STATIONS_PER_BLOCK / 2);
  uint statYoffset = get_local_id(0) % (NR_STATIONS_PER_BLOCK / 2);

  uint loadTime = get_local_id(0) / NR_STATIONS_PER_BLOCK;
  uint loadStat = get_local_id(0) % NR_STATIONS_PER_BLOCK;

  bool doLoad = NR_STATIONS % NR_STATIONS_PER_BLOCK == 0 || firstStation + loadStat < NR_STATIONS;

  uint outputChannel = get_global_id(2);
  uint inputChannel = outputChannel * CHANNEL_INTEGRATION_FACTOR + 1;

#if CORRELATION_MODE & 0x1
  fcomplex vis_0xAx = 0, vis_1xBx = 0;
#endif
#if CORRELATION_MODE & 0x6
  fcomplex vis_0xAy = 0, vis_1xBy = 0;
#endif
#if CORRELATION_MODE & 0x8
  fcomplex vis_0yAy = 0, vis_1yBy = 0;
#endif

  VisType vis_0B = 0;

  uint statY = firstStation + 2 * statYoffset;
  uint statX = firstStation + 2 * statXoffset;
  uint baseline = statX >= statY ? (statX * (statX + 1) / 2) + statY : (statY * (statY + 1) / 2) + statX;

  for (uint major = 0; major < NR_SAMPLES_PER_CHANNEL * CHANNEL_INTEGRATION_FACTOR; major += NR_TIMES_PER_BLOCK) {
    // load data into local memory
    barrier(CLK_LOCAL_MEM_FENCE);

    if (doLoad) {
      fcomplex2 twoSamples = correctedData[inputChannel][loadTime + major][firstStation + loadStat];
      samples[loadStat % 2][0][loadTime][loadStat / 2] = twoSamples.xy;
      samples[loadStat % 2][1][loadTime][loadStat / 2] = twoSamples.zw;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

#if defined NVIDIA_CUDA
#pragma unroll
#endif
    for (uint time = 0; time < NR_TIMES_PER_BLOCK; time ++) {
      fcomplex2 sample_0, sample_1, sample_A, sample_B;
      sample_0.xy = samples[0][0][time][statYoffset];
      sample_0.zw = samples[0][1][time][statYoffset];
      sample_A.xy = samples[0][0][time][statXoffset];
      sample_A.zw = samples[0][1][time][statXoffset];

#if CORRELATION_MODE & 0x1
      vis_0xAx.x += sample_0.x * sample_A.x;
      vis_0xAx.y += sample_0.y * sample_A.x;
      vis_0xAx.x += sample_0.y * sample_A.y;
      vis_0xAx.y -= sample_0.x * sample_A.y;
#endif
#if CORRELATION_MODE & 0x6
      vis_0xAy.x += sample_0.x * sample_A.z;
      vis_0xAy.y += sample_0.y * sample_A.z;
      vis_0xAy.x += sample_0.y * sample_A.w;
      vis_0xAy.y -= sample_0.x * sample_A.w;
#endif
#if CORRELATION_MODE & 0x8
      vis_0yAy.x += sample_0.z * sample_A.z;
      vis_0yAy.y += sample_0.w * sample_A.z;
      vis_0yAy.x += sample_0.w * sample_A.w;
      vis_0yAy.y -= sample_0.z * sample_A.w;
#endif

      sample_B.xy = samples[1][0][time][statXoffset];
      sample_B.zw = samples[1][1][time][statXoffset];

#if CORRELATION_MODE == 0xF
      vis_0B.even += sample_0.xxzz * sample_B.xzxz;
      vis_0B.odd  += sample_0.yyww * sample_B.xzxz;
      vis_0B.even += sample_0.yyww * sample_B.ywyw;
      vis_0B.odd  -= sample_0.xxzz * sample_B.ywyw;
#elif CORRELATION_MODE == 0x9
      vis_0B.even += sample_0.xz * sample_B.xz;
      vis_0B.odd  += sample_0.yw * sample_B.xz;
      vis_0B.even += sample_0.yw * sample_B.yw;
      vis_0B.odd  -= sample_0.xz * sample_B.yw;
#elif CORRELATION_MODE == 0x1
      vis_0B.even += sample_0.x * sample_B.x;
      vis_0B.odd  += sample_0.y * sample_B.x;
      vis_0B.even += sample_0.y * sample_B.y;
      vis_0B.odd  -= sample_0.x * sample_B.y;
#elif CORRELATION_MODE == 0x8
      vis_0B.even += sample_0.z * sample_B.z;
      vis_0B.odd  += sample_0.w * sample_B.z;
      vis_0B.even += sample_0.w * sample_B.w;
      vis_0B.odd  -= sample_0.z * sample_B.w;
#endif

      sample_1.xy = samples[1][0][time][statYoffset];
      sample_1.zw = samples[1][1][time][statYoffset];

#if CORRELATION_MODE & 0x1
      vis_1xBx.x += sample_1.x * sample_B.x;
      vis_1xBx.y += sample_1.y * sample_B.x;
      vis_1xBx.x += sample_1.y * sample_B.y;
      vis_1xBx.y -= sample_1.x * sample_B.y;
#endif
#if CORRELATION_MODE & 0x6
      vis_1xBy.x += sample_1.x * sample_B.z;
      vis_1xBy.y += sample_1.y * sample_B.z;
      vis_1xBy.x += sample_1.y * sample_B.w;
      vis_1xBy.y -= sample_1.x * sample_B.w;
#endif
#if CORRELATION_MODE & 0x8
      vis_1yBy.x += sample_1.z * sample_B.z;
      vis_1yBy.y += sample_1.w * sample_B.z;
      vis_1yBy.x += sample_1.w * sample_B.w;
      vis_1yBy.y -= sample_1.z * sample_B.w;
#endif
    }
  }

  {
    __global fcomplex *dst = (__global fcomplex *) &visibilities[baseline][outputChannel];
    bool doLowerLeftNoConj  = statX >= statY && statX     < NR_STATIONS;
#if CORRELATION_MODE & 0x6
    bool doLowerLeftConj    = statX <= statY && statY     < NR_STATIONS;
#endif

#if CORRELATION_MODE & 0x1
    if (doLowerLeftNoConj)
      dst[0] = vis_0xAx;
#endif

#if CORRELATION_MODE & 0x6
    if (doLowerLeftNoConj)
      dst[1] = vis_0xAy;

    if (doLowerLeftConj)
      dst[2] = (fcomplex) (vis_0xAy.x, -vis_0xAy.y);
#endif

#if CORRELATION_MODE & 0x8
    if (doLowerLeftNoConj)
      dst[(CORRELATION_MODE & 0x6) ? 3 : 1] = vis_0yAy;
#endif
  }

  {
    bool doLowerRight = statX >= statY && statX + 1 < NR_STATIONS;

    if (doLowerRight)
      visibilities[baseline + statX + 1][outputChannel] = vis_0B;
  }

  {
    bool doUpperLeftConj = statX < statY && statY < NR_STATIONS; // transposed

    if (doUpperLeftConj)
#if CORRELATION_MODE == 0xF
      visibilities[baseline + 1][outputChannel] = (fcomplex4) (conj(vis_0B.s01), conj(vis_0B.s45), conj(vis_0B.s23), conj(vis_0B.s67));
#elif CORRELATION_MODE == 0x9
      visibilities[baseline + 1][outputChannel] = (fcomplex2) (conj(vis_0B.s01), conj(vis_0B.s23));
#elif CORRELATION_MODE == 0x1 || CORRELATION_MODE == 0x8
      visibilities[baseline + 1][outputChannel] = conj(vis_0B);
#endif
  }

  {
    __global fcomplex *dst = (__global fcomplex *) &visibilities[baseline + max(statX, statY) + 2][outputChannel];
  bool doUpperRightNoConj = statX >= statY && statX + 1 < NR_STATIONS;
#if CORRELATION_MODE & 0x6
  bool doUpperRightConj   = statX <= statY && statY + 1 < NR_STATIONS;
#endif

#if CORRELATION_MODE & 0x1
    if (doUpperRightNoConj)
      dst[0] = vis_1xBx;
#endif

#if CORRELATION_MODE & 0x6
    if (doUpperRightNoConj)
      dst[(CORRELATION_MODE & 0x1) ? 1 : 0] = vis_1xBy;

    if (doUpperRightConj)
      dst[(CORRELATION_MODE & 0x1) ? 2 : 1] = conj(vis_1xBy);
#endif

#if CORRELATION_MODE & 0x8
    if (doUpperRightNoConj)
      dst[((CORRELATION_MODE & 0x1) ? 1 : 0) + ((CORRELATION_MODE & 0x6) ? 2 : 0)] = vis_1yBy;
#endif
  }
}


#define NR_THREADS (((NR_STATIONS_PER_BLOCK / 2) * ((NR_STATIONS - 1) % NR_STATIONS_PER_BLOCK / 2 + 1) + 63) / 64 * 64)

__kernel
__attribute__((reqd_work_group_size(NR_THREADS, 1, 1)))
__attribute__((vec_type_hint(float)))
void correlateRectangleKernel(VisibilitiesType visibilities,
                              const CorrectedDataType correctedData)
{
#if NR_STATIONS % NR_STATIONS_PER_BLOCK != 0
  __local fcomplex samplesX[2][NR_POLARIZATIONS][NR_TIMES_PER_BLOCK][(NR_STATIONS % NR_STATIONS_PER_BLOCK + 1) / 2];
  __local fcomplex samplesY[2][NR_POLARIZATIONS][NR_TIMES_PER_BLOCK][NR_STATIONS_PER_BLOCK / 2];

  uint firstStationX = NR_STATIONS / NR_STATIONS_PER_BLOCK * NR_STATIONS_PER_BLOCK;
  uint firstStationY = get_global_id(1) * NR_STATIONS_PER_BLOCK;

  uint statXoffset = get_local_id(0) / (NR_STATIONS_PER_BLOCK / 2);
  uint statYoffset = get_local_id(0) % (NR_STATIONS_PER_BLOCK / 2);

  VisType vis_0A = 0, vis_0B = 0, vis_1A = 0, vis_1B = 0;

  uint loadTime = get_local_id(0) / NR_STATIONS_PER_BLOCK;
  uint loadStat = get_local_id(0) % NR_STATIONS_PER_BLOCK;

  uint outputChannel = get_global_id(2);
  uint inputChannel = outputChannel * CHANNEL_INTEGRATION_FACTOR + 1;

  uint statY = firstStationY + 2 * statYoffset;
  uint statX = firstStationX + 2 * statXoffset;
  uint baseline = (statX * (statX + 1) / 2) + statY;

  for (uint major = 0; major < NR_SAMPLES_PER_CHANNEL * CHANNEL_INTEGRATION_FACTOR; major += NR_TIMES_PER_BLOCK) {
    // load data into local memory
    barrier(CLK_LOCAL_MEM_FENCE);

    for (unsigned i = get_local_id(0); i < NR_TIMES_PER_BLOCK * (NR_STATIONS % NR_STATIONS_PER_BLOCK); i += NR_THREADS) {
      uint loadStat = i % (NR_STATIONS % NR_STATIONS_PER_BLOCK);
      uint loadTime = i / (NR_STATIONS % NR_STATIONS_PER_BLOCK);

      fcomplex2 twoSamplesX = correctedData[inputChannel][major + loadTime][firstStationX + loadStat];
      samplesX[loadStat % 2][0][loadTime][loadStat / 2] = twoSamplesX.xy;
      samplesX[loadStat % 2][1][loadTime][loadStat / 2] = twoSamplesX.zw;
    }

    for (unsigned i = get_local_id(0); i < NR_TIMES_PER_BLOCK * NR_STATIONS_PER_BLOCK; i += NR_THREADS) {
      uint loadStat = i % NR_STATIONS_PER_BLOCK;
      uint loadTime = i / NR_STATIONS_PER_BLOCK;

      fcomplex2 twoSamplesY = correctedData[inputChannel][major + loadTime][firstStationY + loadStat];
      samplesY[loadStat % 2][0][loadTime][loadStat / 2] = twoSamplesY.xy;
      samplesY[loadStat % 2][1][loadTime][loadStat / 2] = twoSamplesY.zw;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (statX < NR_STATIONS && statYoffset < NR_STATIONS_PER_BLOCK / 2) {
#if defined NVIDIA_CUDA
#pragma unroll
#endif
      for (uint time = 0; time < NR_TIMES_PER_BLOCK; time ++) {
	fcomplex2 sample_0, sample_1, sample_A, sample_B;
        sample_A.xy = samplesX[0][0][time][statXoffset];
        sample_A.zw = samplesX[0][1][time][statXoffset];
	sample_0.xy = samplesY[0][0][time][statYoffset];
	sample_0.zw = samplesY[0][1][time][statYoffset];
        sample_B.xy = samplesX[1][0][time][statXoffset];
        sample_B.zw = samplesX[1][1][time][statXoffset];
        sample_1.xy = samplesY[1][0][time][statYoffset];
        sample_1.zw = samplesY[1][1][time][statYoffset];

#if CORRELATION_MODE == 0xF
        vis_0A.even += sample_0.xxzz * sample_A.xzxz;
        vis_0A.odd  += sample_0.yyww * sample_A.xzxz;
        vis_0B.even += sample_0.xxzz * sample_B.xzxz;
        vis_0B.odd  += sample_0.yyww * sample_B.xzxz;
        vis_0A.even += sample_0.yyww * sample_A.ywyw;
        vis_0A.odd  -= sample_0.xxzz * sample_A.ywyw;
        vis_0B.even += sample_0.yyww * sample_B.ywyw;
        vis_0B.odd  -= sample_0.xxzz * sample_B.ywyw;
#elif CORRELATION_MODE == 0x9
        vis_0A.even += sample_0.xz * sample_A.xz;
        vis_0A.odd  += sample_0.yw * sample_A.xz;
        vis_0B.even += sample_0.xz * sample_B.xz;
        vis_0B.odd  += sample_0.yw * sample_B.xz;
        vis_0A.even += sample_0.yw * sample_A.yw;
        vis_0A.odd  -= sample_0.xz * sample_A.yw;
        vis_0B.even += sample_0.yw * sample_B.yw;
        vis_0B.odd  -= sample_0.xz * sample_B.yw;
#elif CORRELATION_MODE == 0x1
        vis_0A.even += sample_0.x * sample_A.x;
        vis_0A.odd  += sample_0.y * sample_A.x;
        vis_0B.even += sample_0.x * sample_B.x;
        vis_0B.odd  += sample_0.y * sample_B.x;
        vis_0A.even += sample_0.y * sample_A.y;
        vis_0A.odd  -= sample_0.x * sample_A.y;
        vis_0B.even += sample_0.y * sample_B.y;
        vis_0B.odd  -= sample_0.x * sample_B.y;
#elif CORRELATION_MODE == 0x8
        vis_0A.even += sample_0.z * sample_A.z;
        vis_0A.odd  += sample_0.w * sample_A.z;
        vis_0B.even += sample_0.z * sample_B.z;
        vis_0B.odd  += sample_0.w * sample_B.z;
        vis_0A.even += sample_0.w * sample_A.w;
        vis_0A.odd  -= sample_0.z * sample_A.w;
        vis_0B.even += sample_0.w * sample_B.w;
        vis_0B.odd  -= sample_0.z * sample_B.w;
#endif

#if CORRELATION_MODE == 0xF
        vis_1A.even += sample_1.xxzz * sample_A.xzxz;
        vis_1A.odd  += sample_1.yyww * sample_A.xzxz;
        vis_1B.even += sample_1.xxzz * sample_B.xzxz;
        vis_1B.odd  += sample_1.yyww * sample_B.xzxz;
        vis_1A.even += sample_1.yyww * sample_A.ywyw;
        vis_1A.odd  -= sample_1.xxzz * sample_A.ywyw;
        vis_1B.even += sample_1.yyww * sample_B.ywyw;
        vis_1B.odd  -= sample_1.xxzz * sample_B.ywyw;
#elif CORRELATION_MODE == 0x9
        vis_1A.even += sample_1.xz * sample_A.xz;
        vis_1A.odd  += sample_1.yw * sample_A.xz;
        vis_1B.even += sample_1.xz * sample_B.xz;
        vis_1B.odd  += sample_1.yw * sample_B.xz;
        vis_1A.even += sample_1.yw * sample_A.yw;
        vis_1A.odd  -= sample_1.xz * sample_A.yw;
        vis_1B.even += sample_1.yw * sample_B.yw;
        vis_1B.odd  -= sample_1.xz * sample_B.yw;
#elif CORRELATION_MODE == 0x1
        vis_1A.even += sample_1.x * sample_A.x;
        vis_1A.odd  += sample_1.y * sample_A.x;
        vis_1B.even += sample_1.x * sample_B.x;
        vis_1B.odd  += sample_1.y * sample_B.x;
        vis_1A.even += sample_1.y * sample_A.y;
        vis_1A.odd  -= sample_1.x * sample_A.y;
        vis_1B.even += sample_1.y * sample_B.y;
        vis_1B.odd  -= sample_1.x * sample_B.y;
#elif CORRELATION_MODE == 0x8
        vis_1A.even += sample_1.z * sample_A.z;
        vis_1A.odd  += sample_1.w * sample_A.z;
        vis_1B.even += sample_1.z * sample_B.z;
        vis_1B.odd  += sample_1.w * sample_B.z;
        vis_1A.even += sample_1.w * sample_A.w;
        vis_1A.odd  -= sample_1.z * sample_A.w;
        vis_1B.even += sample_1.w * sample_B.w;
        vis_1B.odd  -= sample_1.z * sample_B.w;
#endif
      }
    }
  }

  if (statX < NR_STATIONS) {
    visibilities[baseline            ][outputChannel] = vis_0A;
    visibilities[baseline +	    1][outputChannel] = vis_1A;
  }

  if (statX + 1 < NR_STATIONS) {
    visibilities[baseline + statX + 1][outputChannel] = vis_0B;
    visibilities[baseline + statX + 2][outputChannel] = vis_1B;
  }
#endif
}


__kernel
//__attribute__((reqd_work_group_size(NR_STATIONS_PER_BLOCK * NR_STATIONS_PER_BLOCK / 4, 1, 1))) // actually decreases performance on AMD!
__attribute__((vec_type_hint(float)))
void correlateSquareKernel(VisibilitiesType visibilities,
                           const CorrectedDataType correctedData)
{
  // this is a complex data format, that avoids bank conflicts
  __local fcomplex samplesX[2][NR_POLARIZATIONS][NR_TIMES_PER_BLOCK][NR_STATIONS_PER_BLOCK / 2];
  __local fcomplex samplesY[2][NR_POLARIZATIONS][NR_TIMES_PER_BLOCK][NR_STATIONS_PER_BLOCK / 2];

  uint block  = get_global_id(1);
  uint blockX = (unsigned) (sqrt(convert_float(8 * block + 1)) - 0.99999f) / 2;
  uint blockY = block - blockX * (blockX + 1) / 2;

  uint firstStationX = (blockX + 1) * NR_STATIONS_PER_BLOCK;
  uint firstStationY = blockY * NR_STATIONS_PER_BLOCK;

  uint statXoffset = get_local_id(0) / (NR_STATIONS_PER_BLOCK / 2);
  uint statYoffset = get_local_id(0) % (NR_STATIONS_PER_BLOCK / 2);

  VisType vis_0A = 0, vis_0B = 0, vis_1A = 0, vis_1B = 0;

  uint loadTime = get_local_id(0) / NR_STATIONS_PER_BLOCK;
  uint loadStat = get_local_id(0) % NR_STATIONS_PER_BLOCK;

  uint outputChannel = get_global_id(2);
  uint inputChannel = outputChannel * CHANNEL_INTEGRATION_FACTOR + 1;

  int statY = firstStationY + 2 * statYoffset;
  uint statX = firstStationX + 2 * statXoffset;
  uint baseline = (statX * (statX + 1) / 2) + statY;

  for (uint major = 0; major < NR_SAMPLES_PER_CHANNEL * CHANNEL_INTEGRATION_FACTOR; major += NR_TIMES_PER_BLOCK) {
    // load data into local memory
    barrier(CLK_LOCAL_MEM_FENCE);

    fcomplex2 twoSamplesX = correctedData[inputChannel][major + loadTime][firstStationX + loadStat];
    fcomplex2 twoSamplesY = correctedData[inputChannel][major + loadTime][firstStationY + loadStat];
    samplesX[loadStat % 2][0][loadTime][loadStat / 2] = twoSamplesX.xy;
    samplesX[loadStat % 2][1][loadTime][loadStat / 2] = twoSamplesX.zw;
    samplesY[loadStat % 2][0][loadTime][loadStat / 2] = twoSamplesY.xy;
    samplesY[loadStat % 2][1][loadTime][loadStat / 2] = twoSamplesY.zw;

    barrier(CLK_LOCAL_MEM_FENCE);

#if defined NVIDIA_CUDA
#pragma unroll
#endif
    for (uint time = 0; time < NR_TIMES_PER_BLOCK; time ++) {
      fcomplex2 sample_0, sample_1, sample_A, sample_B;

      sample_A.xy = samplesX[0][0][time][statXoffset];
      sample_A.zw = samplesX[0][1][time][statXoffset];
      sample_0.xy = samplesY[0][0][time][statYoffset];
      sample_0.zw = samplesY[0][1][time][statYoffset];
      sample_B.xy = samplesX[1][0][time][statXoffset];
      sample_B.zw = samplesX[1][1][time][statXoffset];
      sample_1.xy = samplesY[1][0][time][statYoffset];
      sample_1.zw = samplesY[1][1][time][statYoffset];

#if CORRELATION_MODE == 0xF
      vis_0A.even += sample_0.xxzz * sample_A.xzxz;
      vis_0A.odd  += sample_0.yyww * sample_A.xzxz;
      vis_0B.even += sample_0.xxzz * sample_B.xzxz;
      vis_0B.odd  += sample_0.yyww * sample_B.xzxz;
      vis_1A.even += sample_1.xxzz * sample_A.xzxz;
      vis_1A.odd  += sample_1.yyww * sample_A.xzxz;
      vis_1B.even += sample_1.xxzz * sample_B.xzxz;
      vis_1B.odd  += sample_1.yyww * sample_B.xzxz;

      vis_0A.even += sample_0.yyww * sample_A.ywyw;
      vis_0A.odd  -= sample_0.xxzz * sample_A.ywyw;
      vis_0B.even += sample_0.yyww * sample_B.ywyw;
      vis_0B.odd  -= sample_0.xxzz * sample_B.ywyw;
      vis_1A.even += sample_1.yyww * sample_A.ywyw;
      vis_1A.odd  -= sample_1.xxzz * sample_A.ywyw;
      vis_1B.even += sample_1.yyww * sample_B.ywyw;
      vis_1B.odd  -= sample_1.xxzz * sample_B.ywyw;
#elif CORRELATION_MODE == 0x9
      vis_0A.even += sample_0.xz * sample_A.xz;
      vis_0A.odd  += sample_0.yw * sample_A.xz;
      vis_0B.even += sample_0.xz * sample_B.xz;
      vis_0B.odd  += sample_0.yw * sample_B.xz;
      vis_1A.even += sample_1.xz * sample_A.xz;
      vis_1A.odd  += sample_1.yw * sample_A.xz;
      vis_1B.even += sample_1.xz * sample_B.xz;
      vis_1B.odd  += sample_1.yw * sample_B.xz;

      vis_0A.even += sample_0.yw * sample_A.yw;
      vis_0A.odd  -= sample_0.xz * sample_A.yw;
      vis_0B.even += sample_0.yw * sample_B.yw;
      vis_0B.odd  -= sample_0.xz * sample_B.yw;
      vis_1A.even += sample_1.yw * sample_A.yw;
      vis_1A.odd  -= sample_1.xz * sample_A.yw;
      vis_1B.even += sample_1.yw * sample_B.yw;
      vis_1B.odd  -= sample_1.xz * sample_B.yw;
#elif CORRELATION_MODE == 0x1
      vis_0A.even += sample_0.x * sample_A.x;
      vis_0A.odd  += sample_0.y * sample_A.x;
      vis_0B.even += sample_0.x * sample_B.x;
      vis_0B.odd  += sample_0.y * sample_B.x;
      vis_1A.even += sample_1.x * sample_A.x;
      vis_1A.odd  += sample_1.y * sample_A.x;
      vis_1B.even += sample_1.x * sample_B.x;
      vis_1B.odd  += sample_1.y * sample_B.x;

      vis_0A.even += sample_0.y * sample_A.y;
      vis_0A.odd  -= sample_0.x * sample_A.y;
      vis_0B.even += sample_0.y * sample_B.y;
      vis_0B.odd  -= sample_0.x * sample_B.y;
      vis_1A.even += sample_1.y * sample_A.y;
      vis_1A.odd  -= sample_1.x * sample_A.y;
      vis_1B.even += sample_1.y * sample_B.y;
      vis_1B.odd  -= sample_1.x * sample_B.y;
#elif CORRELATION_MODE == 0x8
      vis_0A.even += sample_0.z * sample_A.z;
      vis_0A.odd  += sample_0.w * sample_A.z;
      vis_0B.even += sample_0.z * sample_B.z;
      vis_0B.odd  += sample_0.w * sample_B.z;
      vis_1A.even += sample_1.z * sample_A.z;
      vis_1A.odd  += sample_1.w * sample_A.z;
      vis_1B.even += sample_1.z * sample_B.z;
      vis_1B.odd  += sample_1.w * sample_B.z;

      vis_0A.even += sample_0.w * sample_A.w;
      vis_0A.odd  -= sample_0.z * sample_A.w;
      vis_0B.even += sample_0.w * sample_B.w;
      vis_0B.odd  -= sample_0.z * sample_B.w;
      vis_1A.even += sample_1.w * sample_A.w;
      vis_1A.odd  -= sample_1.z * sample_A.w;
      vis_1B.even += sample_1.w * sample_B.w;
      vis_1B.odd  -= sample_1.z * sample_B.w;
#endif
    }
  }

  visibilities[baseline            ][outputChannel] = vis_0A;
  visibilities[baseline + statX + 1][outputChannel] = vis_0B;
  visibilities[baseline +         1][outputChannel] = vis_1A;
  visibilities[baseline + statX + 2][outputChannel] = vis_1B;
}
