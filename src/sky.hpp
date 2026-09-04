#pragma once
#include "visual_types.hpp"
#include "dusk/TwilightHostApi.h"
namespace twilight_visuals::sky {
bool read(DuskTwilightSkyboxV1*, const char*, u8, u8, u8);
bool select_layer(int, int);
void shutdown();
}
