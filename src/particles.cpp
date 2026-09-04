#include "particles.hpp"
#include "native_particles.hpp"
#include "blood.hpp"
#include "compat.hpp"
#include "runtime.hpp"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_kankyo_rain.h"
#include "d/d_kankyo_wether.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include <cstring>

namespace twilight_visuals::particles {
namespace {
DEFINE_HOOK(&dKyw_wether_move_draw, WeatherMoveDraw);
DEFINE_HOOK(&dKyw_wether_draw, WeatherDraw);

class VisualHousiPacket final : public dKankyo_housi_Packet {
public:
    void draw() override {
        Mtx drawMtx;
        MTXCopy(j3dSys.getViewMtx(), drawMtx);
        if (g_env_light.camera_water_in_status != 0 && dComIfGd_getView() != nullptr)
            MTXCopy(dComIfGd_getView()->viewMtx, drawMtx);
        const f32 timeScale = runtime_settings().style == Style::AstralPlane ? 0.5f : 1.0f;
        native_particles::draw(drawMtx, &mpResTex, this, timeScale);
    }
};

VisualHousiPacket* g_packet{};
bool g_nativeHousiDrawSuppressed{};
u8 g_savedHousiInitialized{};
bool g_savedNativeState{};
int g_nativeCount{};
u8 g_nativeType{};

void restore_native_particles() {
    if (!g_savedNativeState) return;
    if (g_env_light.mHousiInitialized && g_env_light.mpHousiPacket != nullptr) {
        JKR_DELETE(g_env_light.mpHousiPacket);
        g_env_light.mpHousiPacket = nullptr;
    }
    g_env_light.mHousiInitialized = false;
    g_env_light.mHousiCount = g_nativeCount;
    g_env_light.field_0xea9 = g_nativeType;
    g_savedNativeState = false;
}

HookAction move_pre(ModContext*, void*, void*, void*) {
    if (active()) {
        if (!g_savedNativeState) {
            g_nativeCount = g_env_light.mHousiCount;
            g_nativeType = g_env_light.field_0xea9;
            g_savedNativeState = true;
        }
    } else {
        restore_native_particles();
    }
    return HOOK_CONTINUE;
}

bool enabled() {
    const auto& cfg = runtime_settings();
    return active() && cfg.style != Style::DarkHour;
}

void destroy_packet() {
    if (g_packet != nullptr) {
        JKR_DELETE(g_packet);
        g_packet = nullptr;
    }
}

void move_post(ModContext*, void*, void*, void*) {
    blood::move();
    u8* texture = static_cast<u8*>(dComIfG_getObjectRes("Always", 0x5E));
    if (!enabled() || texture == nullptr) {
        destroy_packet();
        return;
    }
    if (g_packet == nullptr) {
        g_packet = JKR_NEW_ARGS(32) VisualHousiPacket;
        if (g_packet == nullptr) return;
        g_packet->field_0x5de8 = 0.0f;
        g_packet->field_0x10.set(0.0f, 0.0f, 0.0f);
        for (auto& effect : g_packet->mHousiEff) effect.mStatus = 0;
    }
    g_packet->mpResTex = texture;
    dKankyo_housi_Packet* nativePacket = g_env_light.mpHousiPacket;
    const int nativeCount = g_env_light.mHousiCount;
    const u8 nativeType = g_env_light.field_0xea9;
    g_env_light.mpHousiPacket = g_packet;
    g_env_light.mHousiCount = 200;
    g_env_light.field_0xea9 = 0;
    native_particles::move(runtime_settings().style == Style::AstralPlane ? 0.5f : 1.0f);
    g_env_light.mpHousiPacket = nativePacket;
    g_env_light.mHousiCount = nativeCount;
    g_env_light.field_0xea9 = nativeType;
}

HookAction draw_pre(ModContext*, void*, void*, void*) {
    const bool darkHour = active() && runtime_settings().style == Style::DarkHour;
    const bool customParticlesReady = g_packet != nullptr && g_packet->mpResTex != nullptr;
    // All visual styles own the Twilight-particle decision while active.  The
    // Dark Hour intentionally draws no housi packet at all, so its native
    // packet must still be suppressed even though no replacement exists.
    g_nativeHousiDrawSuppressed = darkHour || customParticlesReady;
    if (g_nativeHousiDrawSuppressed) {
        g_savedHousiInitialized = g_env_light.mHousiInitialized;
        g_env_light.mHousiInitialized = 0;
    }
    return HOOK_CONTINUE;
}

void draw_post(ModContext*, void*, void*, void*) {
    blood::draw();
    if (g_nativeHousiDrawSuppressed) {
        g_env_light.mHousiInitialized = g_savedHousiInitialized;
        g_nativeHousiDrawSuppressed = false;
    }
    if (g_packet == nullptr || g_packet->mpResTex == nullptr) return;
    if (g_env_light.camera_water_in_status != 0) {
        dComIfGd_setXluList2DScreen();
        j3dSys.getDrawBuffer(J3DSysDrawBuf_Xlu)->entryImm(g_packet, 0);
        dComIfGd_setList();
    } else if (const char* stage = dComIfGp_getStartStageName();
               stage != nullptr && std::strncmp(stage, "D_MN05", 6) == 0) {
        dComIfGd_getOpaListIndScreen()->entryImm(g_packet, 0);
    } else {
        j3dSys.getDrawBuffer(J3DSysDrawBuf_Xlu)->entryImm(g_packet, 0);
    }
}
}

ModResult install_hooks() {
    ModResult result = mods::hook::add_pre<WeatherMoveDraw>(move_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<WeatherMoveDraw>(move_post);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<WeatherDraw>(draw_pre);
    if (result != MOD_OK) return result;
    return mods::hook::add_post<WeatherDraw>(draw_post);
}
void uninstall_hooks() {
    if (g_nativeHousiDrawSuppressed) {
        g_env_light.mHousiInitialized = g_savedHousiInitialized;
        g_nativeHousiDrawSuppressed = false;
    }
    mods::hook::uninstall<WeatherDraw>();
    mods::hook::uninstall<WeatherMoveDraw>();
    destroy_packet();
    restore_native_particles();
}
void area_reloaded() { g_savedNativeState = false; }
}
