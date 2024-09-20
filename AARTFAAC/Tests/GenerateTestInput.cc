#include "Common/BandPass.h"
#include "Common/FilterBank.h"
#include "Common/Stream/FileStream.h"

#include <byteswap.h>
#include <complex>
#include <cstring>
#include <iostream>
#include <stdint.h>

#define NR_STATIONS		6
#define NR_DIPOLES_PER_STATION	48
#define NR_POLARIZATIONS	2
#define NR_SUBBANDS		8
#define NR_CHANNELS_PER_SUBBAND	64
#define NR_TAPS			16
#define CLOCK_SPEED		200e6
#define SUBBAND_BANDWIDTH	(CLOCK_SPEED / 1024)
#define START_TIME		1410296159.9
#define RUN_TIME		4
#define NR_TIMES		(uint32_t) (RUN_TIME * SUBBAND_BANDWIDTH)
#define NR_SAMPLES_PER_INTEGRATION (196608 / NR_CHANNELS_PER_SUBBAND)
#define NR_INTEGRATIONS		3
#define NR_TIMES_PER_PACKET	2



std::complex<int16_t> (*samples)[NR_SUBBANDS][NR_TIMES][NR_STATIONS][NR_DIPOLES_PER_STATION][NR_POLARIZATIONS];
float filterWeights[NR_CHANNELS_PER_SUBBAND][NR_TAPS];
float bandPass[NR_CHANNELS_PER_SUBBAND];
float delays[NR_STATIONS * NR_DIPOLES_PER_STATION];


void initSamples()
{
#if 0
  (*samples)[5][450000][3][26][1] = std::complex<int16_t>(2, 3);
  (*samples)[5][450000][3][27][0] = std::complex<int16_t>(4, 5);
#else
  double signalFrequency = 8 * (SUBBAND_BANDWIDTH / NR_CHANNELS_PER_SUBBAND);

#pragma omp parallel for
  for (unsigned time = 0; time < NR_TIMES; time ++) {
    double phi = 2 * M_PI * signalFrequency * time / SUBBAND_BANDWIDTH;
    (*samples)[5][time][3][26][0] = std::complex<int16_t>(32767 * cos(phi), 32767 * sin(phi));
    (*samples)[5][time][3][27][1] = std::complex<int16_t>(32767 * cos(phi + .25 * M_PI), 32767 * sin(phi + .25 * M_PI));
  }
#endif 
  
#if 0
  FilterBank filterBank(true, NR_TAPS, NR_CHANNELS_PER_SUBBAND, KAISER);
  filterBank.negateWeights();
  memcpy(filterWeights, filterBank.getWeights().origin(), sizeof filterWeights);

  BandPass::computeCorrectionFactors(bandPass, NR_CHANNELS_PER_SUBBAND);

  if (NR_STATIONS * NR_DIPOLES_PER_STATION == 288) {
    for (unsigned station = 0; station < 48; station ++) {
      delays[station + 0 * 48] = 8.339918e-06;
      delays[station + 1 * 48] = 6.936566e-06;
      delays[station + 2 * 48] = 7.905512e-06;
      delays[station + 3 * 48] = 8.556805e-06;
      delays[station + 4 * 48] = 7.905282e-06;
      delays[station + 5 * 48] = 7.928823e-06;
    }
  } else {
#pragma omp once
#pragma omp critical (cerr)
    std::cerr << "WARNING: I do not know which delays to apply" << std::endl;
  }
#endif
}


void writePacket(Stream &stream, unsigned station, unsigned time)
{
  struct Packet {
    struct Header {
      uint64_t rsp_lane_id : 2;
      uint64_t rsp_sdo_mode : 2;
      uint64_t rsp_rsp_clock : 1;
      uint64_t rsp_reserved_1 : 59;

      uint16_t rsp_station_id;
      uint16_t nof_words_per_block;
      uint16_t nof_blocks_per_packet;

      uint64_t rsp_bsn /*: 50;
      uint64_t rsp_reserved_0 : 13;
      uint64_t rsp_sync : 1*/;
    } __attribute__((packed)) header;

    std::complex<uint16_t> payload[NR_TIMES_PER_PACKET][NR_SUBBANDS][NR_DIPOLES_PER_STATION][NR_POLARIZATIONS];
  } packet;

  memset(&packet.header, 0, sizeof packet.header);

  uint64_t rsp_bsn = (uint64_t) (START_TIME * SUBBAND_BANDWIDTH) + time;

#if !defined __BIG_ENDIAN__
  rsp_bsn = __bswap_64(rsp_bsn);
#endif

  packet.header.rsp_bsn = rsp_bsn;// & 0x3FFFFFFFFFFFFULL;

  for (unsigned t = 0; t < NR_TIMES_PER_PACKET; t ++)
    for (unsigned subband = 0; subband < NR_SUBBANDS; subband ++)
      for (unsigned dipole = 0; dipole < NR_DIPOLES_PER_STATION; dipole ++)
	for (unsigned polarization = 0; polarization < NR_POLARIZATIONS; polarization ++)
	  packet.payload[t][subband][dipole][polarization] = (*samples)[subband][time + t][station][dipole][polarization];

  stream.write(&packet, sizeof packet);
}


void writeSamples()
{
#pragma omp parallel for
  for (unsigned station = 0; station < NR_STATIONS; station ++) {
    std::stringstream fileName;
    fileName << "/tmp/AARTFAAC-" << station << ".raw";
    FileStream stream(fileName.str(), 0644);

    for (unsigned time = 0; time < NR_TIMES; time += NR_TIMES_PER_PACKET) {
      writePacket(stream, station, time);
    }
  }
}


void process(unsigned startIndex)
{
#pragma omp parallel for
  for (unsigned station = 0; station < NR_STATIONS; station ++) {
    for (unsigned time = 0; time < NR_SAMPLES_PER_INTEGRATION; time ++) {
      for (unsigned channel = 0; channel < NR_CHANNELS_PER_SUBBAND; channel ++) {
	for (unsigned tap = 0; tap < NR_TAPS; tap ++) {
	}
      }
    }
  }
}


void process()
{
  for (unsigned startIndex = .1 * SUBBAND_BANDWIDTH; startIndex < NR_TIMES; startIndex += NR_CHANNELS_PER_SUBBAND * NR_SAMPLES_PER_INTEGRATION)
    process(startIndex);
}


int main(int argc, char *argv[])
{
  samples = new std::complex<int16_t>[1][NR_SUBBANDS][NR_TIMES][NR_STATIONS][NR_DIPOLES_PER_STATION][NR_POLARIZATIONS];
  initSamples();
  writeSamples();
  delete [] samples;
  //process();

  return 0;
}
