#ifndef RADIOBLOCKS_VDIFSTREAM_H
#define RADIOBLOCKS_VDIFSTREAM_H

#include "Common/Stream/FileStream.h"
#include "Common/TimeStamp.h"

#include <fstream>
#include <complex>
#include <vector>
#include <boost/multi_array.hpp>
#include <ctime>

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
  
  std::time_t timestamp() const;
  int32_t dataSize() const;
  int32_t samplesPerFrame() const;
  int32_t numberOfChannels() const;
  void decode2bit(char* data, int16_t* result);
};

class VDIFStream : public Stream {
  private:
    std::ifstream file;

    VDIFHeader firstHeader;
    uint32_t _invalidFrames;
    uint32_t _numberOfFrames;

    float DECODER_LEVEL[4] = { -3, -1, 1, 3 };	

  public:
    VDIFStream(std::string inputFile);

    bool readFirstHeader();
    bool checkHeader(VDIFHeader& header);
    void findNextValidHeader(VDIFHeader& header, off_t& offset); 
    void read(char* data);

    void printVDIFHeader(VDIFHeader header);

    uint32_t numberOfFrames() const { return _numberOfFrames; }
    uint32_t invalidFrames() const { return _invalidFrames; }
    
    // NOT USED, they come from Stream class.
    size_t tryWrite(const void *ptr, size_t size) { return 0; }
    size_t tryRead(void *ptr, size_t size) { return 0; }

    ~VDIFStream();
};

#endif //RADIOBLOCKS_VDIFSTREAM_H
