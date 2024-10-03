#ifndef RADIOBLOCKS_VDIFSTREAM_H
#define RADIOBLOCKS_VDIFSTREAM_H

#include "Common/Stream/FileStream.h"
#include "ISBI/Frame.h"

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


class VDIFStream : public Stream{
private:
        //FileStream *in_stream;

	VDIFHeader header;
	VDIFHeader firstHeader;
	VDIFHeader currentHeader;
	int data_frame_size;
	int samples_per_frame;
	int number_of_headers;
        int number_of_frames;

	uint32_t current_timestamp;
	
	string input_file_;
	
public:
     VDIFStream(string input_file);
     uint64_t currentSampleTimestamp;
     void read(Frame &frame);
    void       get_vdif_header();
    void       print_vdif_header();
    int        get_data_frame_size();
    uint8_t    get_log2_nchan();
    int        get_samples_per_frame();
    uint32_t   get_current_timestamp();
    bool       readVDIFHeader(const std::string filePath, VDIFHeader& header, int flag);
    bool       readVDIFData(const std::string filePath, uint32_t frame[][1][16], size_t samples_per_frame,  int flag);

   void print_vdif_header(VDIFHeader header_);
   
    string get_input_file();
    
    void printFirstRow(uint32_t (*frame)[1][16], size_t samples_per_frame) ;

   size_t tryWrite(const void *ptr, size_t size);
   size_t tryRead(void *ptr, size_t size);

    ~VDIFStream() override;
};



#endif //RADIOBLOCKS_VDIFSTREAM_H
