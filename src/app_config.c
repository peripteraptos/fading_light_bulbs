#include "app_config.h"

/* Provide default values for your global config. */
light_config_t g_light_config = {
    .offset1         = 0,
    .offset2         = 50,
    .levelMin        = 5,
    .levelMax        = 254,
    .onTime          = 1,
    .offTime         = 1,
    .transitionTime  = 20,
    .gammaVal        = 2.2,
    .dimmingMode     = DIMMING_MODE_LINEAR
};
