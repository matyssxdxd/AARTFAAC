#ifndef ISBI_VDIFSTREAM_H
#define ISBI_VDIFSTREAM_H

#include "Common/Stream/FileStream.h"
#include "Common/TimeStamp.h"

#include <fstream>
#include <vector>
#include <boost/multi_array.hpp>

struct Header {

    uint32_t sec_from_epoch:30;
    uint32_t legacy_mode:1;
    uint32_t invalid:1;

    uint32_t dataframe_in_second:24;
    uint32_t ref_epoch:6;
    uint32_t unassigned:2;
    
    uint32_t dataframe_length:24;
    uint32_t log2_nchan:5;
    uint32_t version:3;
    
    uint32_t station_id:16;
    uint32_t thread_id:10;
    uint32_t bits_per_sample:5;
    uint32_t data_type:1;
    
    uint32_t user_data_1:24;
    uint32_t edv:8;
    
    uint32_t user_data_2;
    uint32_t user_data_3;
    uint32_t user_data_4;

    Header(): sec_from_epoch(0), legacy_mode(0), invalid(0),
	      dataframe_in_second(0), ref_epoch(0), unassigned(0),
	      dataframe_length(0), log2_nchan(0), version(0), station_id(0),
	      thread_id(0), bits_per_sample(0), data_type(0), user_data_1(0),
	      edv(0), user_data_2(0), user_data_3(0), user_data_4(0) {};
};

struct Frame {
    Header header{};
    uint32_t number_of_samples;
    boost::multi_array<float, 3> data;

    Frame(uint32_t samples_per_frame, uint32_t number_of_threads, uint32_t number_of_channels)
        : samples_per_frame(samples_per_frame),
	  data(boost::extents[samples_per_frame][number_of_threads][number_of_channels]) {}

    TimeStamp timestamp(TimeStamp epoch, double subband_bandwidth) {
        return epoch + static_cast<uint64_t>(header.sec_from_epoch) * subband_bandwidth + header.dataframe_in_second * samples_per_frame;
    }
};

class VDIFStream : public Stream {
private:
	std::ifstream file;

	Header header0;
	
	uint32_t _number_of_headers;
	uint32_t _number_of_frames;
	uint32_t _header0_timestamp;

	uint32_t _bits_per_sample;
	uint32_t _number_of_channels;
	uint32_t _values_per_word;
	uint32_t _payload_nbytes; 
	uint32_t _samples_per_frame;
        uint32_t _number_of_threads;

	// Stuff for decoding the data into 2 bit samples
	float OPTIMAL_2BIT_HIGH = 3.316505f;
	float DECODER_LEVEL[4] = { -OPTIMAL_2BIT_HIGH, -1.0f, 1.0f, OPTIMAL_2BIT_HIGH };
	
	void decode_word(uint32_t word, std::vector<float> &decoded_samples);
	void decode(std::vector<uint32_t> &words, boost::multi_array<float, 3> &data);	

public:
	VDIFStream(std::string input_file);
	void read(Frame &frame);
	void read_header(Header &header, off_t offset);
	void read_data(boost::multi_array<float, 3> &data, off_t offset);
	void print_header(Header header);

	uint32_t number_of_headers();
	uint32_t number_of_frames();
	uint32_t header0_timestamp();

	uint32_t bits_per_sample();
	uint32_t number_of_channels();
	uint32_t values_per_word();
	uint32_t payload_nbytes();
	uint32_t samples_per_frame();
	uint32_t number_of_threads();

	size_t tryWrite(const void *ptr, size_t size);
	size_t tryRead(void *ptr, size_t size);

	~VDIFStream();
};

#endif 
