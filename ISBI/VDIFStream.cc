#include "VDIFStream.h"

#include <iostream>
#include <cstdio>
#include <stdlib.h>
#include <cstdint>
#include <stdexcept>
#include <chrono>
#include <vector>
#include <cstring>

constexpr uint32_t HEADER_SIZE = 32; // bytes
constexpr uint32_t DATA_SIZE = 8000; // bytes

VDIFStream::VDIFStream(std::string inputFile, double sampleRate) 
  : file(inputFile, std::ios::binary), sampleRate(sampleRate), numberOfFrames(0), invalidFrames(0), firstHeaderFound(false) { 
    std::cout << "Created a new VDIFStream object for " << inputFile << std::endl;

    if (!file.is_open()) { throw std::runtime_error("Failed to open " + inputFile + " file!"); }
    if (!readFirstHeader()) { throw std::runtime_error("Could not find a valid header!"); }


    dataSize = firstHeader.dataSize();
    headerSize = firstHeader.headerSize();

    file.clear();
    file.seekg(static_cast<off_t>(numberOfFrames) * (headerSize + dataSize), std::ios::beg);
    if (!file) { throw std::runtime_error("Failed to seek to the first frame!"); }

    std::cout << "VDIF data size: " << dataSize << std::endl;
    std::cout << "VDIF header size: " << headerSize << std::endl;
    std::cout << "Sample rate: " << sampleRate << std::endl;
    std::cout << "VDIF samples per frame: " << firstHeader.samplesPerFrame() << std::endl;
    std::cout << "First timestamp: " << firstHeader.timestamp(sampleRate) << std::endl;
  }

bool VDIFStream::readFirstHeader() {
  while (file) {
    off_t offset = static_cast<off_t>(numberOfFrames) * (HEADER_SIZE + DATA_SIZE);
    file.seekg(offset, std::ios::beg);
    file.read(reinterpret_cast<char*>(&currentHeader), HEADER_SIZE);

    if (!file) return false;

    if (file.gcount() == static_cast<std::streamsize>(HEADER_SIZE)) {
      uint32_t words[4];
      std::memcpy(words, &currentHeader, sizeof(words));

      if (words[0] == 0x11223344 ||
          words[1] == 0x11223344 ||
          words[2] == 0x11223344 ||
          words[3] == 0x11223344) {
        invalidFrames++;
      } else {
        std::memcpy(&firstHeader, &currentHeader, HEADER_SIZE);
        std::cout << "Found first valid header at offset: " << offset << std::endl;
        return true;
     }
    }
  }

  return false;
}

void VDIFStream::read(char* frame) {
  file.read(frame, headerSize + dataSize);
  if (file.gcount() != static_cast<std::streamsize>(headerSize + dataSize)) {
    if (file.eof()) throw EndOfStreamException("VDIFStream::read EOF reached");
    throw EndOfStreamException("VDIFStream::read incomplete frame read");
  }

  std::memcpy(&currentHeader, frame, headerSize);
  if (checkHeader() != HeaderStatus::VALID) {
    const off_t expectedOffset = static_cast<off_t>(numberOfFrames) * (headerSize + dataSize);
    std::cout << "Invalid header found at offset " << expectedOffset << std::endl;
    findNextValidHeader();

    file.read(frame, headerSize + dataSize);
    if (file.gcount() != static_cast<std::streamsize>(headerSize + dataSize)) {
      if (file.eof()) throw EndOfStreamException("VDIFStream::read EOF reached");
      throw EndOfStreamException("VDIFStream::read incomplete frame read");
    }
  }

  numberOfFrames++;
}

void VDIFStream::findNextValidHeader() {
  while (true) {
    numberOfFrames++;
    off_t offset = static_cast<off_t>(numberOfFrames) * (headerSize + dataSize);
    file.seekg(offset, std::ios::beg);

    if (!file) {
      throw EndOfStreamException("VDIFStream::findNextValidHeader: seek failed");
    }

    file.read(reinterpret_cast<char*>(&currentHeader), headerSize);
    if (file.gcount() != static_cast<std::streamsize>(headerSize)) {
      throw EndOfStreamException("VDIFStream::findNextValidHeader: truncated header");
    }

    if (checkHeader() == HeaderStatus::VALID) {
      file.seekg(offset, std::ios::beg);
      return;
    }
  }
}


VDIFStream::~VDIFStream() {
  std::cout << "Total frames read: " <<  numberOfFrames << std::endl;
  file.close();
}

int64_t VDIFHeader::timestamp(double sample_rate) const {
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
    return std::chrono::duration_cast<std::chrono::seconds>(exact_time.time_since_epoch()).count() * sample_rate 
           + dataframe_in_second * samplesPerFrame();
}

void VDIFHeader::decode2bit(const std::array<char, maxPacketSize>& frame,
                            std::vector<int8_t>& out) const {
  static const std::array<int8_t, 256 * 4> decodeLUT = []() {
    std::array<int8_t, 256 * 4> lut{};
    for (unsigned byte = 0; byte < 256; ++byte) {
      lut[byte * 4 + 0] = DECODER_LEVEL_2BIT[(byte >> 0) & 0x3];
      lut[byte * 4 + 1] = DECODER_LEVEL_2BIT[(byte >> 2) & 0x3];
      lut[byte * 4 + 2] = DECODER_LEVEL_2BIT[(byte >> 4) & 0x3];
      lut[byte * 4 + 3] = DECODER_LEVEL_2BIT[(byte >> 6) & 0x3];
    }
    return lut;
  }();

  const std::size_t payloadBytes = static_cast<std::size_t>(dataSize());
  const uint8_t* data =
      reinterpret_cast<const uint8_t*>(frame.data() + headerSize());

  const std::size_t totalSamples =
      static_cast<std::size_t>(samplesPerFrame()) * numberOfChannels();

  const std::size_t decodedSamples =
      std::min(totalSamples, payloadBytes * 4);

  out.assign(totalSamples, 0); // resize + zero-fill remainder

  const std::size_t fullBytes = decodedSamples / 4;
  const std::size_t tail      = decodedSamples % 4;

  for (std::size_t i = 0; i < fullBytes; ++i) {
    const int8_t* decoded = &decodeLUT[static_cast<std::size_t>(data[i]) * 4];
    std::copy_n(decoded, 4, out.begin() + i * 4);
  }

  if (tail != 0) {
    const int8_t* decoded = &decodeLUT[static_cast<std::size_t>(data[fullBytes]) * 4];
    std::copy_n(decoded, tail, out.begin() + fullBytes * 4);
  }
}


