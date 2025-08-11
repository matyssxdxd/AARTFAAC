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

  VDIFHeader() : sec_from_epoch(0),  legacy_mode(0), invalid(0),
  dataframe_in_second(0), ref_epoch(0), unassiged(0),
  dataframe_length(0), log2_nchan(0), version(0), station_id(0),
  thread_id(0), bits_per_sample(0), data_type(0), user_data1(0), edv(0),
  user_data2(0), user_data3(0), user_data4(0){};
  
  std::time_t timestamp() const {
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
};

struct Frame {
  VDIFHeader header{};
  uint32_t number_of_threads;
  uint32_t number_of_channels;
  uint32_t samples_per_frame;
  boost::multi_array<int16_t, 3> samples;

  Frame(uint32_t samples_per_frame, uint32_t number_of_threads, uint32_t number_of_channels)
    : samples_per_frame(samples_per_frame),
    number_of_threads(number_of_threads),
    number_of_channels(number_of_channels),
    samples(boost::extents[samples_per_frame][number_of_threads][number_of_channels]) {}

  TimeStamp timeStamp(double subbandBandwidth) {
    const uint32_t SECONDS_IN_DAY = 86400;
    const uint32_t SECONDS_IN_HOUR = 3600;
    const uint32_t SECONDS_IN_MINUTE = 60;

    uint32_t ref_year = 2000 + header.ref_epoch / 2;
    uint32_t ref_month = 6 * (header.ref_epoch & 1);

    uint32_t ref_seconds = header.sec_from_epoch;

    uint32_t ref_days = ref_seconds / SECONDS_IN_DAY;

    ref_seconds %= SECONDS_IN_DAY;

    uint32_t ref_hours = ref_seconds / SECONDS_IN_HOUR;

    ref_seconds %= SECONDS_IN_HOUR;

    uint32_t ref_minutes = ref_seconds / SECONDS_IN_MINUTE;

    ref_seconds %= SECONDS_IN_MINUTE;

    tm datetime;
    datetime.tm_year = ref_year - 1900;
    datetime.tm_mon = ref_month;
    datetime.tm_mday = ref_days + 1;
    datetime.tm_hour = ref_hours;
    datetime.tm_min = ref_minutes;
    datetime.tm_sec = ref_seconds;
    datetime.tm_isdst = -1;
    mktime(&datetime);

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &datetime);

    TimeStamp timestamp = TimeStamp::fromDate(buffer, subbandBandwidth);
    return timestamp + header.dataframe_in_second * samples_per_frame;
  }

};

class VDIFStream : public Stream {
  private:
    std::ifstream file;

    VDIFHeader firstHeader;
    VDIFHeader currentHeader;
    uint32_t _dataFrameSize; 
    uint32_t _samplesPerFrame; 
    uint32_t _numberOfChannels;
    uint32_t _numberOfFrames;

    float DECODER_LEVEL[4] = { -3, -1, 1, 3 };	

    void decode(std::vector<uint32_t> &words, boost::multi_array<int16_t, 3> &data);

  public:
    VDIFStream(std::string inputFile);
    void read(Frame &frame);
    bool readVDIFHeader(VDIFHeader& header, off_t offset);
    bool readVDIFData(boost::multi_array<int16_t, 3> &frame, off_t offset);

    void printVDIFHeader(VDIFHeader header);

    uint32_t dataFrameSize() const { return _dataFrameSize; }
    uint32_t samplesPerFrame() const { return _samplesPerFrame; }
    uint32_t numberOfChannels() const { return _numberOfChannels; }
    uint32_t numberOfFrames() const { return _numberOfFrames; }
    
    // NOT USED, they come from Stream class.
    size_t tryWrite(const void *ptr, size_t size) { return 0; }
    size_t tryRead(void *ptr, size_t size) { return 0; }

    ~VDIFStream();
};



#endif //RADIOBLOCKS_VDIFSTREAM_H
