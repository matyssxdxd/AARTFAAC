#include <iostream>
#include <fstream>
#include <cstdint>

struct VDIFHeader {
    // Word 0
    uint32_t sec_from_epoch:30;
    uint8_t legacy_mode:1, invalid:1;
    // Word 1
    uint32_t dataframe_in_second:24;
    uint8_t ref_epoch:6, unassiged:2;
    // Word 2
    uint32_t dataframe_length:24;
    uint8_t log2_nchan:5, version:3;
    // Word 3
    uint16_t station_id;
    uint16_t thread_id:10;
    uint8_t bits_per_sample:5, data_type:1;
    // Word 4
    uint32_t user_data1:24;
    uint8_t edv:8;
    // Word 5-7
    uint32_t user_data2, user_data3, user_data4;

    VDIFHeader() : sec_from_epoch(0), legacy_mode(0), invalid(0),
                   dataframe_in_second(0), ref_epoch(0), unassiged(0),
                   dataframe_length(0), log2_nchan(0), version(0), station_id(0),
                   thread_id(0), bits_per_sample(0), data_type(0), user_data1(0), edv(0),
                   user_data2(0), user_data3(0), user_data4(0) {}
};

bool readVDIFHeader(const std::string& filePath, VDIFHeader& header) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return false;
    }
    
    //file.seekg(111733152, std::ios::beg);   // 18786
      file.seekg(114785312, std::ios::beg);  // 18786
    file.read(reinterpret_cast<char*>(&header), sizeof(VDIFHeader));
    if (!file) {
        std::cerr << "Failed to read header from file: " << filePath << std::endl;
        return false;
    }

    return true;
}

int main() {
    std::string filePath = "/mnt/VLBI/data/b004/b004_ib_no0001.m5a";
    VDIFHeader header;

    if (readVDIFHeader(filePath, header)) {
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
        std::cout << "bits_per_sample: " << static_cast<int>(header.bits_per_sample) << std::endl;
        std::cout << "data_type: " << static_cast<int>(header.data_type) << std::endl;
        std::cout << "user_data1: " << header.user_data1 << std::endl;
        std::cout << "edv: " << static_cast<int>(header.edv) << std::endl;
        std::cout << "user_data2: " << header.user_data2 << std::endl;
        std::cout << "user_data3: " << header.user_data3 << std::endl;
        std::cout << "user_data4: " << header.user_data4 << std::endl;
    } else {
        std::cerr << "Error reading VDIF header." << std::endl;
    }

    return 0;
}

