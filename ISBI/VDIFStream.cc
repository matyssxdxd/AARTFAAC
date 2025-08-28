#include "VDIFStream.h"

#include <iostream>
#include <cstdio>
#include <stdlib.h>
#include <cstdint>
#include <stdexcept>
#include <chrono>
#include <vector>
#include <cstring>


int64_t VDIFHeader::timestamp() const {
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

  return std::chrono::duration_cast<std::chrono::seconds>(exact_time.time_since_epoch()).count() * SAMPLE_RATE + dataframe_in_second * samplesPerFrame();
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

void VDIFHeader::decode2bit(const std::vector<char>& frame, std::vector<int16_t>& out) const {
    const std::size_t headerBytes = sizeof(VDIFHeader);
    const std::size_t payloadBytes = static_cast<std::size_t>(dataSize());
    const std::size_t totalBytes   = headerBytes + payloadBytes;

    if (frame.size() < totalBytes) {
        throw std::invalid_argument("decode2bit: frame too small for header + payload");
    }

    const char* dataBuffer = frame.data() + headerBytes;

    const unsigned nchan = static_cast<unsigned>(numberOfChannels());
    const unsigned nsamp = static_cast<unsigned>(samplesPerFrame());

    out.resize(static_cast<std::size_t>(nsamp) * nchan);

    for (unsigned sample = 0; sample < nsamp; ++sample) {
        for (unsigned channel = 0; channel < nchan; ++channel) {
            const unsigned idx = sample * nchan + channel;

            // 4 samples per byte, 2 bits per sample
            const unsigned byteIdx   = idx / 4;
            const unsigned bitOffset = (idx % 4) * 2;

            if (byteIdx < payloadBytes) {
                const uint8_t byte = static_cast<uint8_t>(dataBuffer[byteIdx]);
                const uint8_t code = static_cast<uint8_t>((byte >> bitOffset) & 0b11);
                out[idx] = DECODER_LEVEL_2BIT[code];
            } else {
                out[idx] = 0;
            }
        }
    }
}


VDIFStream::VDIFStream(std::string inputFile) 
  : file(inputFile, std::ios::binary), _numberOfFrames(0), _invalidFrames(0) { 
    if (!file.is_open()) { throw std::runtime_error("Failed to open file!"); }
    if (!readFirstHeader()) { throw std::runtime_error("Could not find a valid header!"); }


    _payloadBytes = static_cast<std::size_t>(firstHeader.dataSize());
    _totalBytes = _headerBytes + _payloadBytes;
}

bool VDIFStream::readFirstHeader() {
  while (file) {
    VDIFHeader header{};

    file.read(reinterpret_cast<char*>(&header), _headerBytes);
    if (!file) { return false; }

    if (file.gcount() == static_cast<std::streamsize>(_headerBytes)) {
      uint32_t words[4];
      static_assert(sizeof(words) == 16, "words must be 16 bytes");
      std::memcpy(words, &header, sizeof(words));
      if (words[0] == 0x11223344 ||
          words[1] == 0x11223344 ||
          words[2] == 0x11223344 ||
          words[3] == 0x11223344) {
        _invalidFrames++;
      } else {
        std::memcpy(&firstHeader, &header, _headerBytes);
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

void VDIFStream::findNextValidHeader(VDIFHeader& header, off_t& offset) {
  while (!checkHeader(header)) {
    _numberOfFrames++;
    offset = static_cast<off_t>(_numberOfFrames) * static_cast<off_t>(_totalBytes);
    file.seekg(offset, std::ios::beg);
    
    file.read(reinterpret_cast<char*>(&header), _headerBytes);
    if (file.gcount() != static_cast<std::streamsize>(_headerBytes)) {
      throw EndOfStreamException("VDIFStream::findNextValidHeader: truncated header"); 
    }
  }
}

void VDIFStream::read(std::vector<char>& frame) {
  if (frame.size() != _totalBytes) { frame.resize(_totalBytes); }

  off_t offset = static_cast<off_t>(_numberOfFrames) * static_cast<off_t>(_totalBytes);

  file.seekg(offset, std::ios::beg);
  if (!file) { throw EndOfStreamException("VDIFStream::read: seek failed"); }

  VDIFHeader header{};
  file.read(reinterpret_cast<char*>(&header), _headerBytes);
  if (file.gcount() != static_cast<std::streamsize>(_headerBytes)) {
    throw EndOfStreamException("VDIFStream::read: truncated header"); 
  }

  if (!checkHeader(header)) {
    _invalidFrames++;
    findNextValidHeader(header, offset);
  }

  std::memcpy(frame.data(), &header, _headerBytes);

  file.seekg(offset + static_cast<std::streamoff>(_headerBytes), std::ios::beg);
  if (!file) { throw EndOfStreamException("VDIFStream::read: seek to payload failed"); }

  file.read(frame.data() + _headerBytes, static_cast<std::streamsize>(_payloadBytes));
  if (file.gcount() != static_cast<std::streamsize>(_payloadBytes)) {
    throw EndOfStreamException("VDIFStream::read: truncated payload");
  }

  _numberOfFrames++;
}

VDIFStream::~VDIFStream() {
  file.close();
}


