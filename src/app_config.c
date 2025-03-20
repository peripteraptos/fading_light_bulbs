#include "app_config.h"

/* Provide default values for your global config. */
light_config_t g_light_config = {
    .offset1         = 0,
    .offset2         = 0,
    .levelMin        = 5,
    .levelMax        = 254,
    .onTime          = 100,
    .offTime         = 100,
    .transitionTime  = 200,
    .gammaVal        = 2.2,
    .dimmingMode     = DIMMING_MODE_LINEAR
};
