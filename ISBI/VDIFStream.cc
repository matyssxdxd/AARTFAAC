#include <iostream>
#include <cstdio>
#include <stdio.h>
#include <stdlib.h>
#include <cstdint>

#include "VDIFStream.h"

#include <stdexcept>
#include <chrono>
#include <cstdint>
#include <vector>
#include <cstring>
#include <iostream>
#include <fstream>


std::time_t VDIFHeader::timestamp() const {
  std::tm date{};
  date.tm_year = 2000 + ref_epoch / 2 - 1900;
  date.tm_mon = (ref_epoch & 1) ? 6 : 0;
  date.tm_mday = 1;
  date.tm_hour = 0;
  date.tm_min = 0;
  date.tm_sec = 0;

  std::time_t time = timegm(&date);
  auto time_point = std::chrono::system_clock::from_time_t(time);
  auto exact_time = time_point + std::chrono::seconds(sec_from_epoch);

  return std::chrono::duration_cast<std::chrono::seconds>(exact_time.time_since_epoch()).count() * 16e6 + dataframe_in_second * 2000;
}

int32_t VDIFHeader::dataSize() const {
  return dataframe_length * 8 - sizeof(VDIFHeader);
}

int32_t VDIFHeader::numberOfChannels() const {
  return 1 << log2_nchan;
}

int32_t VDIFHeader::samplesPerFrame() const {
  int32_t bps = bits_per_sample + 1;
  return dataSize() * 8 / bps / numberOfChannels();
}

void VDIFHeader::decode2bit(char* data, int16_t* result) {
  int16_t DECODER_LEVEL[4] = { -3, -1, 1, 3 };	

  char* dataBuffer = &data[sizeof(VDIFHeader)];

  for (unsigned sample = 0; sample < samplesPerFrame(); sample++) {
    for (unsigned channel = 0; channel < numberOfChannels(); channel++) {
      unsigned byteIdx = (sample * numberOfChannels() + channel) / 4; // 4 = samples per byte
      unsigned bitOffset = ((sample * numberOfChannels() + channel) % 4) * 2;

      if (byteIdx < dataSize()) {
        uint8_t byte = static_cast<uint8_t>(dataBuffer[byteIdx]);
        uint8_t sample2bit = (byte >> bitOffset) & 0b11;
        result[sample * numberOfChannels() + channel] = DECODER_LEVEL[sample2bit];
      }
    }
  }
}


VDIFStream::VDIFStream(std::string inputFile) 
  : file(inputFile, std::ios::binary), _numberOfFrames(0), _invalidFrames(0) { 
    if (!file.is_open()) { throw std::runtime_error("Failed to open file!"); }
    if (!readFirstHeader()) { throw std::runtime_error("Could not find a valid header!"); }
}

bool VDIFStream::readFirstHeader() {
  while (file) {
    VDIFHeader header;
    char* headerBuffer = reinterpret_cast<char*>(&header);

    file.read(headerBuffer, 16);
    if (!file) { return false; }

    size_t nwords = file.gcount() / 4;

    if (header.legacy_mode == 0) {
      file.read(headerBuffer + 16, 16);
      nwords = file.gcount() / 4;
    }

    if (nwords == 4) {
      if (((uint32_t *)headerBuffer)[0] == 0x11223344 ||
          ((uint32_t *)headerBuffer)[1] == 0x11223344 ||
          ((uint32_t *)headerBuffer)[2] == 0x11223344 ||
          ((uint32_t *)headerBuffer)[3] == 0x11223344) {
        _invalidFrames++;
      } else {
        std::memcpy(&firstHeader, &header, sizeof(VDIFHeader));
        return true;
      }
    }
  }
  return false;
}


bool VDIFStream::checkHeader(VDIFHeader& header) {
  if (header.legacy_mode != firstHeader.legacy_mode) return false;
  if (header.ref_epoch != firstHeader.ref_epoch) return false;
  if (header.dataframe_length != firstHeader.dataframe_length) return false;
  if (header.log2_nchan != firstHeader.log2_nchan) return false;
  if (header.version != firstHeader.version) return false;
  if (header.station_id != firstHeader.station_id) return false;
  if (header.bits_per_sample != firstHeader.bits_per_sample) return false;
  if (header.data_type != firstHeader.data_type) return false;
  return true;
}

void VDIFStream::read(char* data) {
  off_t offset = _numberOfFrames * (firstHeader.dataSize() + sizeof(VDIFHeader));
  file.seekg(offset, std::ios::beg);
  VDIFHeader header;
  char* headerBuffer = reinterpret_cast<char*>(&header);

  file.read(headerBuffer, sizeof(VDIFHeader));
  if (!file) { throw EndOfStreamException("read"); }

  if (!checkHeader(header)) {
    _invalidFrames++;
    // find next valid header
  }

  std::memcpy(data, &header, sizeof(VDIFHeader));
  file.read(&data[sizeof(VDIFHeader)], firstHeader.dataSize());
  _numberOfFrames++;
}

void VDIFStream::printVDIFHeader(VDIFHeader header) {
  std::cout << "----- VDIF HEADER -----" << std::endl;
  std::cout << "sec_from_epoch: " << header.sec_from_epoch << std::endl;
  std::cout << "legacy_mode: " << static_cast<int>(header.legacy_mode) << std::endl;
  std::cout << "invalid: " << static_cast<int>(header.invalid) << std::endl;
  std::cout << "dataframe_in_second: " << header.dataframe_in_second << std::endl;
  std::cout << "ref_epoch: " << static_cast<int>(header.ref_epoch) << std::endl;
  std::cout << "dataframe_length: " << header.dataframe_length << std::endl;
  std::cout << "log2_nchan: " << static_cast<int>(header.log2_nchan) << std::endl;
  std::cout << "version: " << static_cast<int>(header.version) << std::endl;
  std::cout << "station_id: " << header.station_id << std::endl;
  std::cout << "thread_id: " << header.thread_id << std::endl;
  std::cout << "data_type: " << static_cast<int>(header.data_type) << std::endl;
  std::cout << "user_data1: " << header.user_data1 << std::endl;
  std::cout << "edv: " << static_cast<int>(header.edv) << std::endl;
  std::cout << "user_data2: " << header.user_data2 << std::endl;
  std::cout << "user_data3: " << header.user_data3 << std::endl;
  std::cout << "user_data4: " << header.user_data4 << std::endl;
}

VDIFStream::~VDIFStream() {
  file.close();
}


