#include "hooks.hpp"

#include "runtime.hpp"

#include "mods/service.hpp"
#include "mods/svc/hook.hpp"

namespace twilight_visuals {
namespace {
DEFINE_HOOK_SYMBOL("dusk::speedrun::resetForSpeedrunMode", void(), SpeedrunReset);
DEFINE_HOOK_SYMBOL("dusk::speedrun::restoreFromSpeedrunMode", void(), SpeedrunRestore);

void speedrun_reset_post(ModContext*, void*, void*, void*) { set_speedrun_suppressed(true); }
void speedrun_restore_post(ModContext*, void*, void*, void*) { set_speedrun_suppressed(false); }
}  // namespace

ModResult install_hooks() {
    ModResult result = mods::hook::add_post<SpeedrunReset>(speedrun_reset_post);
    if (result != MOD_OK) return result;
    return mods::hook::add_post<SpeedrunRestore>(speedrun_restore_post);
}

void uninstall_hooks() {
    mods::hook::uninstall<SpeedrunRestore>();
    mods::hook::uninstall<SpeedrunReset>();
}

}  // namespace twilight_visuals


