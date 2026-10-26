#ifndef CROP_MODEL_H
#define CROP_MODEL_H

#if __has_include("Sensors/sensors.h")
#include "Sensors/sensors.h"
#else
#include "sensors.h"
#endif

float model_stress_probability(const crop_reading_t *reading);

#endif
