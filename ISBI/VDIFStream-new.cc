#include "VDIFStream.h"

#include <iostream>
#include <stdexcept>

/*
 * TODOS: * Implement print_header
 *        * Check includes
 */

VDIFStream::VDIFStream(std::string input_file)
    : file(input_file, std::ios::binary),
      _number_of_headers(0),
      _number_of_frames(0),
      _number_of_threads(1) {
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file.");
    }

    read_header(header0, 0);

    _bits_per_sample = header0.bits_per_sample + 1;
    _number_of_channels = 1 << header0.log2_nchan;
    _values_per_word = 32 / bits_per_sample / 1;
    _payload_nbytes = header0.dataframe_length * 8 - 32;
    _samples_per_frame = payload_nbytes / 4 * values_per_word / number_of_channels;
}

void VDIFStream::read(Frame &frame) {
    off_t header_offset = (_number_of_headers + _number_of_frames) * (_payload_nbytes + sizeof(Header));    
    read_header(frame.header, header_offset);
    _number_of_headers++;

    off_t data_offset = _number_of_headers * sizeof(Header) + _number_of_frames * _payload_nbytes;
    read_data(frame.data, data_offset);
    _number_of_frames++;
}

void VDIFStream::read_header(Header &header, off_t offset) {
    file.seekg(offest);

    if (file.peek() == EOF) {
        throw EndOfStreamException("Reached the end of the data (VDIFStream::read_header)");
    }

    file.read(reinterpret_cast<char *>(&header), sizeof(Header));

    if (!file) {
        throw std::runtime_error("Failed to read header from the file");
    }
}

void VDIFStream::read_data(boost::multi_array<float, 3> &data, off_t offset) {
    file.seekg(offset);

    if (file.peek() == EOF) {
        throw EndOfStreamException("Reached the end of the data (VDIFStream::read_data)");
    }

    std::vector<uint32_t> buffer(_payload_nbytes / sizeof(uint32_t);
    file.read(reinterpet_cast<char *>(buffer.data()), _payload_nbytes);

    if (!file) {
        throw std::runtime_error("Failed to read data from the file");
    }

    decode(buffer, frame);
}

void VDIFStream::decode_word(uint32_t word, std::vector<float> &decoded_samples) {
    for (unsigned i = 0; i < _number_of_channels; i++) {
        decoded_samples[i] = DECODER_LEVEL[(word >> (i * 2)) & 0b11];
    }
}

void VDIFStream::decode(std::vector<uint32_t> &words, boost::multi_array<float, 3> &data) {
    for (unsigned i = 0; i < _samples_per_frame; i++) {
        std::vector<float> decoded_samples(_number_of_channels);
	decode_word(words[i], decoded_samples);

	// Would need to update this code in a case where _number_of_threads is something different than 1
	for (unsigned j = 0; j < _number_of_channels; j++) {
	    data[i][0][j] = decoded_samples[j];
	}
    }
}

uint32_t VDIFStream::number_of_headers() {
    return _number_of_headers;
}

uint32_t VDIFStream::number_of_frames() {
    return _number_of_frames;
}

uint32_t VDIFStream::header0_timestamp() {
    return header0.sec_from_epoch;
}

uint32_t VDIFStream::bits_per_sample() {
    return _bits_per_sample;
}

uint32_t VDIFStream::number_of_channels() {
    return _number_of_channels;
}

uint32_t VDIFStream::values_per_word() {
    return _values_per_word;
}

uint32_t VDIFStream::payload_nbytes() {
    return _payload_nbytes;
}

uint32_t VDIFStream::samples_per_frame() {
    return _samples_per_frame;
}

uint32_t VDIFStream::number_of_threads() {
    return _number_of_threads;
}

size_t VDIFStream::tryWrite(const void *ptr; size_t size) {
    return 0;
}

size_t VDIFStream::tryRead(void *ptr, size_t size) {
    return 0;
}

VDIFStream::~VDIFStream() {
    file.close();
}
