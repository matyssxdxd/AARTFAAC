#ifndef RADIOBLOCKS_VDIFSTREAM_H
#define RADIOBLOCKS_VDIFSTREAM_H

#include "Common/Stream/FileStream.h"
#include "Common/TimeStamp.h"

#include <fstream>
#include <complex>
#include <vector>
#include <ctime>

// Temporary until cross-correlations are fixed, can be read from VDIF file and/or Parset.
constexpr int SAMPLE_RATE = 16000000;

static constexpr int16_t DECODER_LEVEL_2BIT[] = { -3, -1, 1, 3 };

struct VDIFHeader {
  // Word 0
  uint32_t      sec_from_epoch:30;
  uint32_t       legacy_mode:1, invalid:1;
  // Word 1
  uint32_t      dataframe_in_second:24;
  uint32_t       ref_epoch:6, unassiged:2;
  // Word 2
  uint32_t      dataframe_length:24;
  uint32_t       log2_nchan:5, version:3;
  // Word 3
  uint32_t      station_id:16, thread_id:10;
  uint32_t       bits_per_sample:5, data_type:1;
  // Word 4
  uint32_t      user_data1:24;
  uint32_t       edv:8;
  // Word 5-7
  uint32_t      user_data2,user_data3,user_data4;
  
  int64_t timestamp() const;
  int32_t dataSize() const;
  int32_t samplesPerFrame() const;
  int32_t numberOfChannels() const;
  void decode2bit(const std::vector<char>& frame, std::vector<int16_t>& out) const;

  friend std::ostream& operator<<(std::ostream& os, const VDIFHeader& header) {
    os << "----- VDIF HEADER -----" << std::endl;
    os << "sec_from_epoch: " << header.sec_from_epoch << std::endl;
    os << "legacy_mode: " << static_cast<int>(header.legacy_mode) << std::endl;
    os << "invalid: " << static_cast<int>(header.invalid) << std::endl;
    os << "dataframe_in_second: " << header.dataframe_in_second << std::endl;
    os << "ref_epoch: " << static_cast<int>(header.ref_epoch) << std::endl;
    os << "dataframe_length: " << header.dataframe_length << std::endl;
    os << "log2_nchan: " << static_cast<int>(header.log2_nchan) << std::endl;
    os << "version: " << static_cast<int>(header.version) << std::endl;
    os << "station_id: " << header.station_id << std::endl;
    os << "thread_id: " << header.thread_id << std::endl;
    os << "data_type: " << static_cast<int>(header.data_type) << std::endl;
    os << "user_data1: " << header.user_data1 << std::endl;
    os << "edv: " << static_cast<int>(header.edv) << std::endl;
    os << "user_data2: " << header.user_data2 << std::endl;
    os << "user_data3: " << header.user_data3 << std::endl;
    os << "user_data4: " << header.user_data4 << std::endl;
    return os;
  }

};

class VDIFStream : public Stream {
  private:
    std::ifstream file;

    uint32_t _invalidFrames;
    uint32_t _numberOfFrames;

    std::size_t _headerBytes = sizeof(VDIFHeader);
    std::size_t _payloadBytes = 0;
    std::size_t _totalBytes = 0; 
  public:
    VDIFStream(std::string inputFile);

    VDIFHeader firstHeader;

    bool readFirstHeader();
    bool checkHeader(VDIFHeader& header);
    void findNextValidHeader(VDIFHeader& header, off_t& offset); 
    void read(std::vector<char>& frame);

    void printVDIFHeader(VDIFHeader header);

    uint32_t numberOfFrames() const { return _numberOfFrames; }
    uint32_t invalidFrames() const { return _invalidFrames; }
    
    // NOT USED, they come from Stream class.
    size_t tryWrite(const void *ptr, size_t size) { return 0; }
    size_t tryRead(void *ptr, size_t size) { return 0; }

    ~VDIFStream();
};

#endif //RADIOBLOCKS_VDIFSTREAM_H
