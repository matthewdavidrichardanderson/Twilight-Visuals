#pragma once
#include <mtx.h>
#include "compat.hpp"
// Existing MFB interpolation API; no Twilight implementation in the host.
namespace dusk::frame_interp {
inline float get_interpolation_step() { return twilight_visuals::compat::host_api()->interpolationStep(); }
inline bool is_enabled() { return twilight_visuals::compat::host_api()->interpolationEnabled(); }
inline bool is_sim_frame() { return twilight_visuals::compat::host_api()->simulationFrame(); }
inline void record_final_mtx(Mtx m, const void* key) { twilight_visuals::compat::host_api()->recordMatrix(m, key); }
inline bool lookup_replacement(const void* key, Mtx m) { return twilight_visuals::compat::host_api()->lookupMatrix(key, m); }
}
