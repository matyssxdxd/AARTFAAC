#ifndef ISBI_FRAME_H
#define ISBI_FRAME_H

struct Frame {
	uint32_t (*dataArray)[1][16];
	uint64_t timeStamp;

	Frame() {
		dataArray = new uint32_t[2000][1][16];
		timeStamp = 0;
	};

};

#endif
