#ifndef ISBI_SAMPLE_H
#define ISBI_SAMPLE_H

struct Sample {
	uint32_t data[1][16];
	uint64_t seconds_from_epoch;
};

#endif
