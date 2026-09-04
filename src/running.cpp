#include "running.hpp"
#include "runtime.hpp"
#include "compat.hpp"
#include "d/actor/d_a_alink.h"
#include "m_Do/m_Do_controller_pad.h"
#include "SSystem/SComponent/c_m3d.h"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_draw.h"
#include "run_hold.hpp"
namespace twilight_visuals::running {
namespace {
RunHold aButton;
daAlink_c* inputPlayer = nullptr;
bool inputSampled = false;
daAlink_c* rollChainPlayer = nullptr;
bool previousUpdateStartedRolling = false;
bool justFinishedRoll = false;
bool enabled(daAlink_c* p) {
    // Targeting owns A's normal roll/evade actions, even without a locked actor.
    // Also respect switch-targeting, where lock-on outlasts the physical press.
    return active() && runtime_settings().skywardSwordRunning &&
        !p->checkWolf() && !p->checkEventRun() &&
        !mDoCPd_c::getHoldL(PAD_1) && !p->checkAttentionLock();
}
bool held(daAlink_c* p) {
    return enabled(p) && inputPlayer == p && aButton.running() && p->doButton();
}
bool moving(daAlink_c* p) { return held(p) && p->mProcID == daAlink_c::PROC_MOVE && p->mStickValue > 0.1f; }
bool water(daAlink_c* p) {
    if (!held(p) || !p->checkMagicArmorWearAbility() || p->checkMagneBootsOn() || p->mWaterY == -G_CM3D_F_INF) return false;
    const f32 offset = p->mWaterY - p->current.pos.y;
    return offset > -120.0f && offset < 180.0f;
}
bool grounded(daAlink_c* p) { return (p->mLinkAcch.ChkGroundHit() || water(p)) && !p->checkModeFlg(daAlink_c::MODE_SWIMMING); }
bool snow(daAlink_c* p) { return held(p) && p->checkSnowCode() && !p->checkBootsOrArmorHeavy(); }
DEFINE_HOOK(&daAlink_c::procFrontRollInit, RollInit);
DEFINE_HOOK(&daAlink_c::procFrontRoll, RollUpdate);
DEFINE_HOOK(&daAlink_c::execute, PlayerExecute);
DEFINE_HOOK(&daAlink_c::checkMoveDoAction, MoveAction);
DEFINE_HOOK(&dMeter2Draw_c::getActionString, ActionString);
DEFINE_HOOK(&daAlink_c::procStepMove, StepMove);
DEFINE_HOOK(&daAlink_c::setSandShapeOffset, SandSink);
daAlink_c* sprintRollPlayer = nullptr;
f32 sprintRollSpeed = 0.0f;

HookAction input_pre(ModContext*, void* args, void*, void*) {
    if (!compat::host_api()->simulationFrame()) return HOOK_CONTINUE;
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (!enabled(p)) aButton = {};
    const bool rolling = p->mProcID == daAlink_c::PROC_FRONT_ROLL;
    justFinishedRoll = rollChainPlayer == p && previousUpdateStartedRolling && !rolling;
    rollChainPlayer = p;
    previousUpdateStartedRolling = rolling;
    inputSampled = false;
    return HOOK_CONTINUE;
}

HookAction move_action_pre(ModContext*, void* args, void* retval, void*) {
    if (!compat::host_api()->simulationFrame() || inputSampled) return HOOK_CONTINUE;
    inputSampled = true;
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (inputPlayer != p) {
        aButton = {};
        inputPlayer = p;
    }
    const bool normalMovement = p->mProcID == daAlink_c::PROC_MOVE ||
        p->mProcID == daAlink_c::PROC_WAIT;
    // A new roll-chain press must reach vanilla on its trigger frame, not
    // enter Ready and wait for release. Vanilla still enforces cancel timing.
    if (enabled(p) && dComIfGp_getDoStatus() == BUTTON_STATUS_UNK_121 &&
        nativeRollChainPress(p->doTrigger(),
            p->mProcID == daAlink_c::PROC_FRONT_ROLL,
            justFinishedRoll && normalMovement)) {
        aButton = {};
        return HOOK_CONTINUE;
    }
    // Mirror procWolfRollAttackCharge's release-before-charge decision, using
    // its ready interpolation duration without changing Link's animation.
    // This hook runs after the game builds mItemButton/mItemTrigger.
    const bool eligible = enabled(p) &&
        ((normalMovement && dComIfGp_getDoStatus() == BUTTON_STATUS_UNK_121) ||
         (aButton.running() && (normalMovement ||
             p->mProcID == daAlink_c::PROC_FRONT_ROLL ||
             p->mProcID == daAlink_c::PROC_STEP_MOVE)));
    if (aButton.update(p->doTrigger(), p->doButton(), eligible,
                       p->mpHIO->mWolf.mWlAttack.m.mReadyInterpolation)) {
        if (grounded(p) && !p->checkMagneBootsOn()) {
            if (p->checkInputOnR()) p->shape_angle.y = p->mMoveAngle;
            *static_cast<BOOL*>(retval) = p->procFrontRollInit();
            return HOOK_SKIP_ORIGINAL;
        }
    }
    if (eligible && aButton.state != RunHold::State::Idle) {
        *static_cast<BOOL*>(retval) = FALSE;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction sand_sink_pre(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    // Only actual SS running on sand bypasses sinking. Snow, other forms,
    // ordinary movement and released A retain the native sink calculation.
    if (!moving(p) || !p->mLinkAcch.ChkGroundHit() ||
        p->mGndPolyAtt0 != 3 || p->checkSnowCode()) return HOOK_CONTINUE;
    p->mSinkShapeOffset = 0.0f;
    p->field_0x2fc9 = 0x10;
    p->mZ2Link.setSinkDepth(-1);
    return HOOK_SKIP_ORIGINAL;
}

void step_move_post(ModContext*, void* args, void*, void*) {
    if (!compat::host_api()->simulationFrame()) return;
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (inputPlayer != p) return;
    // Observe release even before the climb's normal action/cancel window.
    if (!enabled(p) || !p->doButton()) {
        aButton = {};
        return;
    }
    // Leave the climb animation and placement alone; resume speed on exit.
    if (moving(p) && grounded(p)) {
        p->mNormalSpeed = 45.0f * (p->checkEquipHeavyBoots() ? 0.70f : 1.0f);
    }
}

void action_string_post(ModContext*, void* args, void* retval, void*) {
    // Display-only override: leave the native Roll status intact for gameplay.
    if (mods::arg<u8>(args, 1) != BUTTON_STATUS_UNK_121) return;
    auto* p = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (!p || !enabled(p)) return;
    if (p->mProcID != daAlink_c::PROC_MOVE && p->mProcID != daAlink_c::PROC_WAIT) return;
    static char runText[] = "Run";
    *static_cast<char**>(retval) = runText;
}

HookAction roll_pre(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    // Capture before vanilla initialization caps the roll's entry speed.
    sprintRollPlayer = moving(p) && grounded(p) ? p : nullptr;
    sprintRollSpeed = sprintRollPlayer ? p->mNormalSpeed : 0.0f;
    return HOOK_CONTINUE;
}

void roll_post(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (p != sprintRollPlayer) return;
    // Never carry momentum into a crash, another action, or a disabled mod.
    if (!enabled(p) || p->mProcID != daAlink_c::PROC_FRONT_ROLL ||
        !p->mLinkAcch.ChkGroundHit() || p->mLinkAcch.ChkWallHit()) {
        sprintRollPlayer = nullptr;
        return;
    }
    cM3dGPla poly;
    if (p->getSlidePolygon(&poly)) {
        sprintRollPlayer = nullptr;
        return;
    }
    p->mNormalSpeed = sprintRollSpeed;
}
s32 invoke(void* raw, DuskPlayerEvent point, f32 value) {
    auto* p = static_cast<daAlink_c*>(raw);
    if (!p) return 0;
    switch (point) {
    case DuskPlayer_RunEnabled: return held(p);
    case DuskPlayer_IsRunning: return moving(p);
    case DuskPlayer_HeavyBootsRunning: return moving(p) && p->checkEquipHeavyBoots() && grounded(p);
    case DuskPlayer_SnowRunning: return snow(p);
    case DuskPlayer_WaterRunning: return water(p);
    case DuskPlayer_CanStepUp: return moving(p) && p->mLinkAcch.ChkWallHit() && !p->checkMagneBootsOn() &&
        !p->checkModeFlg(daAlink_c::MODE_SWIMMING) &&
        (p->field_0x2f91 == 7 || p->field_0x2f91 == 8 || p->field_0x2f91 == 9);
    case DuskPlayer_JumpMode: return moving(p) && !p->checkMagneBootsOn() && !p->checkModeFlg(daAlink_c::MODE_SWIMMING) ? 2 : 0;
    case DuskPlayer_UpdateRunSpeed:
        if (moving(p) && grounded(p)) p->mNormalSpeed = 45.0f * (p->checkEquipHeavyBoots() ? 0.70f : 1.0f);
        break;
    case DuskPlayer_UpdateSnowSpeed:
        if (snow(p)) { p->mStickValue = value; p->mHeavySpeedMultiplier = 1.0f; }
        break;
    case DuskPlayer_HandleRunAction:
        // A release restores contextual input even if the move-action hook is
        // bypassed by a higher-priority interaction during this update.
        if (inputPlayer == p && aButton.running() && !p->doButton()) aButton = {};
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
    mods::hook::add_pre<SandSink>(sand_sink_pre);
    mods::hook::add_post<StepMove>(step_move_post);
    mods::hook::add_post<ActionString>(action_string_post);
    mods::hook::add_pre<MoveAction>(move_action_pre);
    mods::hook::add_pre<PlayerExecute>(input_pre);
    mods::hook::add_pre<RollInit>(roll_pre);
    mods::hook::add_post<RollInit>(roll_post);
    mods::hook::add_post<RollUpdate>(roll_post);
    const DuskPlayerHooksV1 hooks{invoke,animation};
    compat::host_api()->setPlayerHooks(&hooks);
}
bool is_running() {
    auto* p = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    return p && moving(p) && grounded(p);
}
void shutdown() {
    mods::hook::uninstall<SandSink>();
    mods::hook::uninstall<StepMove>();
    mods::hook::uninstall<ActionString>();
    rollChainPlayer = nullptr;
    previousUpdateStartedRolling = false;
    justFinishedRoll = false;
    mods::hook::uninstall<MoveAction>();
    aButton = {};
    inputPlayer = nullptr;
    mods::hook::uninstall<PlayerExecute>();
    sprintRollPlayer = nullptr;
    mods::hook::uninstall<RollUpdate>();
    mods::hook::uninstall<RollInit>();
    compat::host_api()->setPlayerHooks(nullptr);
}
}
