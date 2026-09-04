#include "running.hpp"
#include "runtime.hpp"
#include "compat.hpp"
#include "d/actor/d_a_alink.h"
#include "m_Do/m_Do_controller_pad.h"
#include "SSystem/SComponent/c_m3d.h"
namespace twilight_visuals::running {
namespace {
bool enabled(daAlink_c* p) { return active() && runtime_settings().skywardSwordRunning && !p->checkWolf() && !p->checkEventRun(); }
bool held(daAlink_c* p) { return enabled(p) && mDoCPd_c::getHoldA(PAD_1); }
bool moving(daAlink_c* p) { return held(p) && p->mProcID == daAlink_c::PROC_MOVE && p->mStickValue > 0.1f; }
bool water(daAlink_c* p) {
    if (!held(p) || !p->checkMagicArmorWearAbility() || p->checkMagneBootsOn() || p->mWaterY == -G_CM3D_F_INF) return false;
    const f32 offset = p->mWaterY - p->current.pos.y;
    return offset > -120.0f && offset < 180.0f;
}
bool grounded(daAlink_c* p) { return (p->mLinkAcch.ChkGroundHit() || water(p)) && !p->checkModeFlg(daAlink_c::MODE_SWIMMING); }
bool snow(daAlink_c* p) { return held(p) && p->checkSnowCode() && !p->checkBootsOrArmorHeavy(); }
s32 invoke(void* raw, DuskPlayerEvent point, f32 value) {
    auto* p = static_cast<daAlink_c*>(raw);
    if (!p) return 0;
    switch (point) {
    case DuskPlayer_RunEnabled: return enabled(p);
    case DuskPlayer_IsRunning: return moving(p);
    case DuskPlayer_HeavyBootsRunning: return moving(p) && p->checkEquipHeavyBoots() && grounded(p);
    case DuskPlayer_SnowRunning: return snow(p);
    case DuskPlayer_WaterRunning: return water(p);
    case DuskPlayer_CanStepUp: return moving(p) && p->mLinkAcch.ChkWallHit() && !p->checkMagneBootsOn() &&
        !p->checkModeFlg(daAlink_c::MODE_SWIMMING) &&
        (p->field_0x2f91 == 7 || p->field_0x2f91 == 8 || p->field_0x2f91 == 9);
    case DuskPlayer_JumpMode: return moving(p) && !p->checkMagneBootsOn() && !p->checkModeFlg(daAlink_c::MODE_SWIMMING) ? 2 : 0;
    case DuskPlayer_UpdateRunSpeed:
        if (moving(p) && grounded(p)) p->mNormalSpeed = 37.0f * (p->checkEquipHeavyBoots() ? 0.70f : 1.0f);
        break;
    case DuskPlayer_UpdateSnowSpeed:
        if (snow(p)) { p->mStickValue = value; p->mHeavySpeedMultiplier = 1.0f; }
        break;
    case DuskPlayer_HandleRunAction:
        if (moving(p) && grounded(p) && !p->checkMagneBootsOn()) {
            if (p->spActionTrigger()) return p->procFrontRollInit();
            if (p->swordTrigger()) {
                if (p->mEquipItem == 0x103 && !p->checkEquipAnime()) return p->procCutJumpInit(FALSE);
                if (p->mEquipItem != 0x103 && p->checkSwordGet() && !p->checkEquipAnime() &&
                    !p->checkNotBattleStage() && (!p->checkModeFlg(0x40000) || p->checkEquipHeavyBoots())) {
                    p->swordEquip(TRUE); return 1;
                }
            }
        }
        break;
    case DuskPlayer_UpdateWaterHeight:
        if (water(p)) { p->current.pos.y = p->mWaterY + 2.0f; p->speed.y = 0; p->mFallHeight = p->current.pos.y; }
        break;
    case DuskPlayer_ApplyLedgeBoost: if (value == 2 && enabled(p)) p->mNormalSpeed *= 1.50f; break;
    case DuskPlayer_TryStepUp:
        if (invoke(p, DuskPlayer_CanStepUp,0) && p->field_0x3078 > p->mpHIO->mWallHang.m.grab_input_time) return p->procStepMoveInit();
        break;
    case DuskPlayer_SuppressWaterFall: if (water(p)) { p->speed.y = 0; return 1; } break;
    }
    return 0;
}
void animation(void* raw, DuskPlayerAnimationEvent point, s32* value, f32* rate) {
    auto* p = static_cast<daAlink_c*>(raw);
    if (!p || !value) return;
    if (point == DuskPlayerAnimation_HeavyMovement && (invoke(p, DuskPlayer_HeavyBootsRunning,0) || (snow(p) && moving(p)))) *value = 0;
    if (point == DuskPlayerAnimation_Run && moving(p)) *value = daAlink_c::ANM_RUN_B;
    if (point == DuskPlayerAnimation_HeavyRun && invoke(p, DuskPlayer_HeavyBootsRunning,0)) {
        *value = p->checkSlope() ? daAlink_c::ANM_WALK_SLOPE : daAlink_c::ANM_RUN_B;
        if (rate) *rate *= 0.70f;
    }
}
}
void initialize() {
    const DuskPlayerHooksV1 hooks{invoke,animation};
    compat::host_api()->setPlayerHooks(&hooks);
}
void shutdown() { compat::host_api()->setPlayerHooks(nullptr); }
}
