#include <iostream>
#include <cstdio>
#include <stdio.h>
#include <stdlib.h>
#include <cstdint>

#include <Common/Stream/Descriptor.h>
#include "Common/Stream/FileStream.h"

#include "VDIFStream.h"

#include <stdexcept>
#include <chrono>
#include <cstdint>
#include <vector>
#include <cstring>
#include <iostream>
#include <fstream>


using namespace std;

VDIFStream::VDIFStream(string input_file)
	: file(input_file, std::ios::binary) {
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file");
	}
	   	
    number_of_headers = 0;
    number_of_frames = 0;    
    input_file_ = input_file;

    bool read_vdif = readVDIFHeader(input_file, first_header, 0);

    if (read_vdif) {
        print_vdif_header();
		uint32_t bits_per_sample = first_header.bits_per_sample + 1;
		number_of_channels = 1 << first_header.log2_nchan;
		uint32_t values_per_word = 32 / bits_per_sample / 1;
		uint32_t payload_nbytes = first_header.dataframe_length * 8 - 32;
		samples_per_frame = payload_nbytes / 4 * values_per_word / number_of_channels;
		data_frame_size = static_cast<int>(first_header.dataframe_length) * 8 - 32 +  16 * static_cast<int>(first_header.legacy_mode);
       	current_timestamp = first_header.sec_from_epoch;
    }else {
        std::cerr << "Error reading VDIF header." << std::endl;
    }
}

    size_t VDIFStream::tryWrite(const void *ptr, size_t size){ return 0;}
    size_t VDIFStream::tryRead(void *ptr, size_t size){ return 0; }


string VDIFStream::get_input_file(){ return   input_file_;  }


bool VDIFStream::readVDIFHeader(const std::string filePath, VDIFHeader& header, off_t offset) {
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return false;
    }
    
    file.seekg(offset, std::ios::beg);
   
   file.read(reinterpret_cast<char*>(&header), sizeof(VDIFHeader)); 

    if (file.peek() == EOF) {
	throw EndOfStreamException("readVDIFHeader");
    }	

    if (!file) {
        std::cerr << "Failed to read header from file: " << filePath << std::endl;
        return false;
    }
    
    return true;
}


bool VDIFStream::readVDIFData(const std::string filePath, boost::multi_array<int16_t, 3> &frame, size_t samples_per_frame, off_t offset) {
//bool VDIFStream::readVDIFData(const std::string filePath, std::complex<int16_t> (*frame)[1][2], size_t samples_per_frame,  off_t offset {

    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return false;
    }
    
  
    file.seekg(offset, std::ios::beg);
    size_t dataSize = first_header.dataframe_length * 8 - 32; // 1004 * 8 = 8032 - 32 = 8000 bytes
    std::vector<uint32_t> buffer(dataSize / sizeof(uint32_t));
    //size_t dataSize = samples_per_frame * 1 * 2 * sizeof(std::complex<int16_t>);

    if (file.peek() == EOF) {
	throw EndOfStreamException("readVDIFData");
    }
   
    file.read(reinterpret_cast<char*>(buffer.data()), dataSize);

    decode(buffer, frame);

    return true;
}


#if 0
void VDIFStream::printFirstRow(std::complex<int16_t> (*frame)[1][16], size_t samples_per_frame) {
    if (samples_per_frame > 0) {
        // Allocate memory for the float32 array
        float (*float_frame)[1][16] = new float[samples_per_frame][1][16];

        // Convert values from std::complex<int16_t> to float32
        for (size_t i = 0; i < samples_per_frame; ++i) {
            for (size_t j = 0; j < 16; ++j) {
                float_frame[i][0][j] = static_cast<float>(frame[i][0][j]);
            }
        }


        // Print the first row of the float32 array
        std::cout << "First row of the float32 array:" << std::endl;
        for (size_t j = 0; j < 16; ++j) {
            std::cout << float_frame[0][0][j] << " ";
        }
        std::cout << std::endl;

        // Clean up the allocated memory
        delete[] float_frame;
    }
}
#endif


void VDIFStream::read(Frame &frame){
    readVDIFHeader(get_input_file(), frame.header, (sizeof(VDIFHeader)+ data_frame_size) * number_of_frames);
    number_of_headers += 1;


    readVDIFData(get_input_file(), frame.samples,  samples_per_frame, sizeof(VDIFHeader) * (number_of_headers) + data_frame_size * number_of_frames);
    number_of_frames += 1;

    current_timestamp = current_header.sec_from_epoch;
}


int VDIFStream::get_data_frame_size(){
    return  data_frame_size;	
}


uint8_t VDIFStream::get_log2_nchan(){
    return first_header.log2_nchan;
}

int VDIFStream::get_samples_per_frame(){
    return samples_per_frame;
}


uint32_t VDIFStream::get_current_timestamp(){
    return current_timestamp;
}


void VDIFStream::print_vdif_header() {
        std::cout << "VDIF Header read successfully:" << std::endl;
        std::cout << "sec_from_epoch: " << first_header.sec_from_epoch << std::endl;
        std::cout << "legacy_mode: " << static_cast<int>(first_header.legacy_mode) << std::endl;
        std::cout << "invalid: " << static_cast<int>(first_header.invalid) << std::endl;
        std::cout << "dataframe_in_second: " << first_header.dataframe_in_second << std::endl;
        std::cout << "ref_epoch: " << static_cast<int>(first_header.ref_epoch) << std::endl;
        std::cout << "dataframe_length: " << first_header.dataframe_length << std::endl;
        std::cout << "log2_nchan: " << static_cast<int>(first_header.log2_nchan) << std::endl;
        std::cout << "version: " << static_cast<int>(first_header.version) << std::endl;
        std::cout << "station_id: " << first_header.station_id << std::endl;
        std::cout << "thread_id: " << first_header.thread_id << std::endl;
        std::cout << "bits_per_sample: " << static_cast<int>(first_header.bits_per_sample) << std::endl;
        std::cout << "data_type: " << static_cast<int>(first_header.data_type) << std::endl;
        std::cout << "user_data1: " << first_header.user_data1 << std::endl;
        std::cout << "edv: " << static_cast<int>(first_header.edv) << std::endl;
        std::cout << "user_data2: " << first_header.user_data2 << std::endl;
        std::cout << "user_data3: " << first_header.user_data3 << std::endl;
        std::cout << "user_data4: " << first_header.user_data4 << std::endl;
}


void VDIFStream::print_vdif_header(VDIFHeader header) {
        std::cout << "VDIF Header read successfully:" << std::endl;
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
    const int samples_per_word = 16 / number_of_channels;
    
    for (size_t word_idx = 0; word_idx < words.size(); word_idx++) {
        uint32_t word = words[word_idx];
        for (int sample_idx = 0; sample_idx < samples_per_word; sample_idx++) {
            for (size_t channel = 0; channel < number_of_channels; channel++) {
                // Calculate bit position
                int bit_pos = (sample_idx * number_of_channels + channel) * 2;
                
                // Extract 2-bit sample and decode it
                uint32_t sample = (word >> bit_pos) & 0b11;
                data[word_idx * samples_per_word + sample_idx][0][channel] = DECODER_LEVEL[sample];
            }
        }
    }
}

void VDIFStream::decode_word(uint32_t word, std::vector<int16_t> &data) {
    for (int i = 0; i < number_of_channels; i++) {
        data[i] = DECODER_LEVEL[(word >> (i * 2)) & 0b11];
    }
}

VDIFStream::~VDIFStream() {
    file.close();
}


