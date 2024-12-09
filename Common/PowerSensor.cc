#include "Common/PowerSensor.h"

#if defined MEASURE_POWER
PowerSensor3::PowerSensor powerSensor("/dev/ttyACM0");
#endif
