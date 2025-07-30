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

VDIFStream::VDIFStream(std::string inputFile)
  : file(inputFile, std::ios::binary) {
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file");
    }

    _numberOfFrames = 0;    

    if (readVDIFHeader(firstHeader, 0)) {
      uint32_t bitsPerSample = firstHeader.bits_per_sample + 1;
      _numberOfChannels = 1 << firstHeader.log2_nchan;
      uint32_t valuesPerWord = 32 / bitsPerSample / 1;
      uint32_t payloadNBytes = firstHeader.dataframe_length * 8 - 32;
      _samplesPerFrame = payloadNBytes / 4 * valuesPerWord / _numberOfChannels;
      _dataFrameSize = static_cast<uint32_t>(firstHeader.dataframe_length) * 8 - 32 + 16 * static_cast<uint32_t>(firstHeader.legacy_mode);
    } else {
      throw std::runtime_error("Failed to read VDIF header.");
    }
  }

bool VDIFStream::readVDIFHeader(VDIFHeader& header, off_t offset) {
  if (!file.is_open()) {
    return false;
  }

  file.seekg(offset, std::ios::beg);

  file.read(reinterpret_cast<char*>(&header), sizeof(VDIFHeader)); 

  if (file.peek() == EOF) {
    throw EndOfStreamException("readVDIFHeader");
  }	

  if (!file) {
    std::cerr << "Failed to read header from file" << std::endl;
    return false;
  }

  return true;
}


bool VDIFStream::readVDIFData(boost::multi_array<int16_t, 3> &frame, off_t offset) {
  if (!file.is_open()) {
    std::cerr << "Failed to open file" << std::endl;
    return false;
  }


  file.seekg(offset, std::ios::beg);
  size_t dataSize = firstHeader.dataframe_length * 8 - 32; // 1004 * 8 = 8032 - 32 = 8000 bytes
  std::vector<uint32_t> buffer(dataSize / sizeof(uint32_t));

  if (file.peek() == EOF) {
    throw EndOfStreamException("readVDIFData");
  }

  file.read(reinterpret_cast<char*>(buffer.data()), dataSize);

  decode(buffer, frame);

  return true;
}

void VDIFStream::read(Frame &frame){
  off_t offset = (sizeof(VDIFHeader) + _dataFrameSize) * _numberOfFrames;
  if (!readVDIFHeader(frame.header, offset)) {
    std::cout << "readVDIFHeader\n";
    return;
  }

  if (!readVDIFData(frame.samples, offset + sizeof(VDIFHeader))) {
    std::cout << "readVDIFData\n";
    return;
  }

  _numberOfFrames += 1;
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


void VDIFStream::decode(std::vector<uint32_t> &words, boost::multi_array<int16_t, 3> &data) {
  const int samples_per_word = 16 / _numberOfChannels;

  for (size_t word_idx = 0; word_idx < words.size(); word_idx++) {
    uint32_t word = words[word_idx];
    for (int sample_idx = 0; sample_idx < samples_per_word; sample_idx++) {
      for (size_t channel = 0; channel < _numberOfChannels; channel++) {
        // Calculate bit position
        int bit_pos = (sample_idx * _numberOfChannels + channel) * 2;

        // Extract 2-bit sample and decode it
        uint32_t sample = (word >> bit_pos) & 0b11;
        data[word_idx * samples_per_word + sample_idx][0][channel] = DECODER_LEVEL[sample];
      }
    }
  }
}

VDIFStream::~VDIFStream() {
  file.close();
}


