#ifndef ISBI_FRAME_H
#define ISBI_FRAME_H

struct Frame {
	uint32_t (*samples)[1][16];
	uint64_t (*sample_timestamps);

	Frame() {
		samples = new uint32_t[2000][1][16];
		sample_timestamps = new uint64_t[2000];
	};
};

#endif
