#if !defined COMMON_POWER_SENSOR_H
#define COMMON_POWER_SENSOR_H

#include "Common/Config.h"

#if defined MEASURE_POWER
#include <PowerSensor.hpp>

extern PowerSensor3::PowerSensor powerSensor;

#endif
#endif
