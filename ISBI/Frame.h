#ifndef ISBI_FRAME_H
#define ISBI_FRAME_H

#include "ISBI/Sample.h"

struct Frame {
	Sample *samples;

	Frame() {
		samples = new Sample[2000];
	};
};

#endif
