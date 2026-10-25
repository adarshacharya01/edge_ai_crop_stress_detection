#include "Model/model.h"

#include <math.h>
#include "model_params.h"

float model_stress_probability(const crop_reading_t *reading)
{
    const float inputs[4] = {
        reading->soil_pct,
        reading->temperature_c,
        reading->humidity_pct,
        log1pf(fmaxf(0.0f, reading->light_lux))
    };
    float logit = MODEL_B;
    for (unsigned i = 0; i < 4; ++i)
        logit += MODEL_W[i] * (inputs[i] - MODEL_MEAN[i]) / MODEL_SCALE[i];
    logit = fmaxf(-30.0f, fminf(30.0f, logit));
    return 1.0f / (1.0f + expf(-logit));
}
