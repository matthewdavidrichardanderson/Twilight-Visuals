#pragma once

#include "mods/api.h"

namespace twilight_visuals::music {
ModResult initialize();
void shutdown();
void set_volume(float value);
void prepare_scene();
void sequence(bool scene, bool eligible, int mode, float gain, bool scope, bool battle, float battleVolume);
}
