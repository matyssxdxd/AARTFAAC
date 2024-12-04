#ifndef RADIOBLOCKS_VDIFSTREAM_H
#define RADIOBLOCKS_VDIFSTREAM_H

#include "Common/Stream/FileStream.h"
#include "Common/TimeStamp.h"

#include <fstream>
#include <complex>
#include <vector>
#include <boost/multi_array.hpp>
#include <ctime>

using namespace std;

struct VDIFHeader {
    // Word 0
    uint32_t      sec_from_epoch:30;
    uint8_t       legacy_mode:1, invalid:1;
    // Word 1
    uint32_t      dataframe_in_second:24;
    uint8_t       ref_epoch:6, unassiged:2;
    // Word 2
    uint32_t      dataframe_length:24;
    uint8_t       log2_nchan:5, version:3;
    // Word 3
    uint16_t      station_id:16, thread_id:10;
    uint8_t       bits_per_sample:5, data_type:1;
    // Word 4
    uint32_t      user_data1:24;
    uint8_t       edv:8;
    // Word 5-7
    uint32_t      user_data2,user_data3,user_data4;

    VDIFHeader() : sec_from_epoch(0),  legacy_mode(0), invalid(0),
                   dataframe_in_second(0), ref_epoch(0), unassiged(0),
                   dataframe_length(0), log2_nchan(0), version(0), station_id(0),
                   thread_id(0), bits_per_sample(0), data_type(0), user_data1(0), edv(0),
                   user_data2(0), user_data3(0), user_data4(0){};
};

struct Frame {
        VDIFHeader header{};
	uint32_t number_of_threads;
	uint32_t number_of_channels;
	uint32_t samples_per_frame;
	boost::multi_array<std::complex<int16_t>, 3> samples;

        Frame(uint32_t samples_per_frame, uint32_t number_of_threads, uint32_t number_of_channels)
	    : samples_per_frame(samples_per_frame),
	      number_of_threads(number_of_threads),
	      number_of_channels(number_of_channels),
	      samples(boost::extents[samples_per_frame][number_of_threads][number_of_channels]) {}
	
	TimeStamp timeStamp(TimeStamp epoch, double subbandBandwidth, unsigned sample_in_frame) {
       	    // unsigned ref_year = 2000 * header.ref_epoch / 2;
	    // unsigned ref_month = 6 * (header.ref_epoch & 1);
	    // unsigned n = header.sec_from_epoch;
	    // unsigned day = n / (24 * 3600);
	    // 
	    // n = n % (24 * 3600);
	    // int hour = n / 3600;

	    // n %= 3600;
	    // int minutes = n / 60;

	    // n %= 60;
	    // int seconds = n;

	    // tm datetime;
	    // datetime.tm_year = ref_year - 1900;
	    // datetime.tm_mon = ref_month;
	    // datetime.tm_mday = day;
	    // datetime.tm_hour = hour;
	    // datetime.tm_min = minutes;
	    // datetime.tm_sec = seconds;
	    // datetime.tm_isdst = -1;
	    // mktime(&datetime);

	    // char buffer[20];
	    // strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &datetime);
	    // std::cout << buffer << "\n";
	    // TimeStamp timestamp = TimeStamp::fromDate(buffer, subbandBandwidth);
	    // std::cout << timestamp << "\n";
	    // return timestamp;
       	    return epoch + static_cast<uint64_t>(header.sec_from_epoch) * subbandBandwidth + header.dataframe_in_second * samples_per_frame;
	}
};

class VDIFStream : public Stream{
private:
	std::ifstream file;

	VDIFHeader first_header;
	VDIFHeader current_header;
	int data_frame_size;
	int samples_per_frame;
	int number_of_headers;
        int number_of_frames;
	uint32_t number_of_channels;
//	float OPTIMAL_2BIT_HIGH = 3.316505f;
	int16_t OPTIMAL_2BIT_HIGH = 10;
//	float DECODER_LEVEL[4] = { -OPTIMAL_2BIT_HIGH, -1.0f, 1.0f, OPTIMAL_2BIT_HIGH };
	int16_t DECODER_LEVEL[4] { -OPTIMAL_2BIT_HIGH, -3, 3, OPTIMAL_2BIT_HIGH };
	void decode_word(uint32_t word, std::vector<std::complex<int16_t>> &samples);
	void decode(std::vector<uint32_t> &words, boost::multi_array<std::complex<int16_t>, 3> &data);

	uint32_t current_timestamp;
	
	string input_file_;
	
public:
     VDIFStream(string input_file);
     void read(Frame &frame);
    void       get_vdif_header();
    void       print_vdif_header();
    int        get_data_frame_size();
    uint8_t    get_log2_nchan();
    int        get_samples_per_frame();
    uint32_t   get_current_timestamp();
    bool       readVDIFHeader(const std::string filePath, VDIFHeader& header, off_t flag);
    bool       readVDIFData(const std::string filePath, boost::multi_array<std::complex<int16_t>, 3> &frame, size_t samples_per_frame,  off_t offset);
    //bool       readVDIFData(const std::string filePath, std::complex<int16_t> (*frame)[1][2], size_t samples_per_frame,  off_t offset);

   void print_vdif_header(VDIFHeader header_);
   
    string get_input_file();
    
    //void printFirstRow(std::complex<int16_t> (*frame)[1][16], size_t samples_per_frame) ;

   size_t tryWrite(const void *ptr, size_t size);
   size_t tryRead(void *ptr, size_t size);

    ~VDIFStream() override;
};



#endif //RADIOBLOCKS_VDIFSTREAM_H
