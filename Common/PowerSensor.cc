#include "Common/PowerSensor.h"

#if defined MEASURE_POWER
PowerSensor::PowerSensor powerSensor("/dev/ttyACM0");
#endif
