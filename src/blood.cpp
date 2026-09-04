#include "blood.hpp"
#include "runtime.hpp"
#include "d/d_kankyo_wether.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "SSystem/SComponent/c_math.h"
#include "d/d_com_inf_game.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_kankyo.h"
#include "d/d_kankyo_rain.h"
#include "f_op/f_op_camera_mng.h"
#include <cstring>
namespace twilight_visuals::blood {
namespace {
bool dark_hour_moon_enabled() { return active() && runtime_settings().style == Style::DarkHour; }
struct DarkHourBloodMark {
    cXyz position;
    f32 radius[16];
    cXyz surfaceNormal;
    f32 surfaceD;
    f32 extent;
    f32 rotation;
    f32 sheenAngle;
    bool active;
};

class DarkHourBloodPacket : public J3DPacket {
public:
    void draw() override {
        if (!dark_hour_moon_enabled()) return;

        j3dSys.reinitGX();
        GXSetNumChans(1);
        GXSetChanCtrl(GX_COLOR0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
                     GX_LIGHT_NULL, GX_DF_CLAMP, GX_AF_NONE);
        GXSetNumTexGens(0);
        GXSetNumTevStages(1);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
        GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                        GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
        GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                        GX_TRUE, GX_TEVPREV);
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
        GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_DISABLE);
        GXSetZCompLoc(GX_TRUE);
        GXSetCullMode(GX_CULL_NONE);
        GXSetAlphaCompare(GX_GREATER, 4, GX_AOP_AND, GX_ALWAYS, 0);
        GXSetNumIndStages(0);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
        GXClearVtxDesc();
        GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
        GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
        GXSetCurrentMtx(GX_PNMTX0);

        // Reapply the scene fog after reinitializing GX. Without this, blood
        // remained fully saturated at long range and appeared on top of the
        // Dark Hour distance fog instead of receding with the floor beneath it.
        dKy_GxFog_set();

        for (const DarkHourBloodMark& mark : marks) {
            if (!mark.active) continue;
            const auto surfaceY = [&mark](f32 x, f32 z) {
                return (-mark.surfaceNormal.x * x - mark.surfaceNormal.z * z -
                        mark.surfaceD) / mark.surfaceNormal.y + 0.8f;
            };
            const auto drawPool = [&mark, &surfaceY](f32 scale, f32 offsetX, f32 offsetZ,
                                                     f32 height, GXColor center, GXColor rim) {
                const f32 centerX = mark.position.x + offsetX;
                const f32 centerZ = mark.position.z + offsetZ;
                GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, 18);
                GXPosition3f32(centerX, surfaceY(centerX, centerZ) + height, centerZ);
                GXColor4u8(center.r, center.g, center.b, center.a);
                for (int edge = 0; edge <= 16; ++edge) {
                    const int sample = edge & 15;
                    const f32 angle = mark.rotation + sample * 0.39269908f;
                    const f32 x = centerX + sinf(angle) * mark.radius[sample] * scale;
                    const f32 z = centerZ + cosf(angle) * mark.radius[sample] * scale;
                    GXPosition3f32(x, surfaceY(x, z) + height, z);
                    GXColor4u8(rim.r, rim.g, rim.b, rim.a);
                }
                GXEnd();
            };

            // Layer near-black coagulated edges beneath a translucent burgundy
            // body. Offset lobes break up the concentric decal appearance, and
            // small contained droplets make the perimeter read as a spill.
            drawPool(1.0f, 0.0f, 0.0f, 0.0f, {34, 0, 3, 225}, {16, 0, 2, 165});
            drawPool(0.91f, -mark.radius[2] * 0.018f, mark.radius[10] * 0.015f,
                     0.18f, {78, 1, 7, 205}, {45, 0, 4, 175});
            drawPool(0.47f, mark.radius[5] * 0.07f, -mark.radius[13] * 0.04f,
                     0.32f, {104, 4, 10, 110}, {67, 1, 6, 80});
            drawPool(0.105f, mark.extent * 0.73f, mark.extent * 0.16f,
                     0.08f, {65, 0, 5, 205}, {24, 0, 2, 145});
            drawPool(0.075f, -mark.extent * 0.62f, mark.extent * 0.43f,
                     0.08f, {72, 1, 6, 195}, {25, 0, 2, 135});

            // Wet blood should read as a shallow reflective puddle, not as a
            // solid decal. These soft tapered patches act as a small water
            // texture while keeping every highlight grounded on the sampled
            // collision plane. They fade at the edge so no hard line is left
            // across the puddle.
            const f32 reflectionScale = mark.extent / 300.0f;
            const f32 sheenCos = cosf(mark.sheenAngle);
            const f32 sheenSin = sinf(mark.sheenAngle);
            const f32 centerX = mark.position.x;
            const f32 centerZ = mark.position.z;
            const auto wetPoint = [&](f32 along, f32 across, f32 height) {
                const f32 x = centerX + sheenCos * along - sheenSin * across;
                const f32 z = centerZ + sheenSin * along + sheenCos * across;
                return cXyz(x, surfaceY(x, z) + height, z);
            };
            const auto drawWetPatch = [&](f32 along, f32 across, f32 radiusAlong,
                                          f32 radiusAcross, f32 phase, GXColor color,
                                          f32 height) {
                GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, 9);
                const cXyz center = wetPoint(along, across, height);
                GXPosition3f32(center.x, center.y, center.z);
                GXColor4u8(color.r, color.g, color.b, color.a);
                for (int point = 0; point <= 7; ++point) {
                    const f32 angle = phase + point * 0.78539816f;
                    const f32 irregular = 0.82f + 0.18f * sinf(angle * 3.0f + phase);
                    const cXyz edge = wetPoint(along + cosf(angle) * radiusAlong * irregular,
                                               across + sinf(angle) * radiusAcross * irregular,
                                               height);
                    GXPosition3f32(edge.x, edge.y, edge.z);
                    GXColor4u8(color.r, color.g, color.b,
                               static_cast<u8>(color.a * 0.08f));
                }
                GXEnd();
            };

            // Broad, low-alpha warm reflections suggest a wet surface without
            // making the blood glow or drawing bright colored lines over it.
            drawWetPatch(-mark.extent * 0.20f, mark.extent * 0.09f,
                          mark.extent * 0.16f, 12.0f * reflectionScale,
                          mark.sheenAngle, {190, 72, 76, 30}, 0.55f);
            drawWetPatch(mark.extent * 0.02f, -mark.extent * 0.15f,
                          mark.extent * 0.10f, 8.0f * reflectionScale,
                          mark.sheenAngle + 0.8f, {225, 125, 128, 18}, 0.7f);
        }
        J3DShape::resetVcdVatCache();
    }

    DarkHourBloodMark marks[160] = {};
    u32 nextMark = 0;
};

static DarkHourBloodPacket s_darkHourBloodPacket;

static u32 dark_hour_blood_random(u32& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static f32 dark_hour_blood_random_unit(u32& state) {
    return static_cast<f32>(dark_hour_blood_random(state) & 0x00FFFFFF) /
           static_cast<f32>(0x01000000);
}

static bool dark_hour_blood_footprint_is_walkable(const cXyz& center, f32 ground,
                                                   const f32 (&radius)[16], f32 rotation) {
    // Validate the center plus the complete rim. This keeps puddles off walls,
    // ledges, holes, steep terrain and geometry on a different vertical level.
    for (int sample = 0; sample < 16; ++sample) {
        const f32 angle = rotation + sample * 0.39269908f;
        cXyz probe(center.x + sinf(angle) * radius[sample], ground + 120.0f,
                   center.z + cosf(angle) * radius[sample]);
        dBgS_GndChk check;
        check.SetPos(&probe);
        const f32 rimGround = dComIfG_Bgsp().GroundCross(&check);
        cM3dGPla rimSurface;
        if (rimGround == -G_CM3D_F_INF || fabsf(rimGround - ground) > 28.0f ||
            !dComIfG_Bgsp().GetTriPla(check, &rimSurface) || rimSurface.mNormal.y < 0.78f) {
            return false;
        }
    }
    return true;
}

static void dark_hour_blood_move() {
    static u32 randomState = 0xD44B100Du;
    static s8 previousRoom = -128;
    static char previousStage[16] = {};

    if (!dark_hour_moon_enabled()) {
        previousRoom = -128;
        previousStage[0] = '\0';
        for (DarkHourBloodMark& mark : s_darkHourBloodPacket.marks) mark.active = false;
        s_darkHourBloodPacket.nextMark = 0;
        return;
    }

    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const char* stage = dComIfGp_getStartStageName();
    if (player == NULL || stage == NULL) return;

    const s8 room = dComIfGp_roomControl_getStayNo();
    if (room != previousRoom || strncmp(previousStage, stage, sizeof(previousStage) - 1) != 0) {
        previousRoom = room;
        strncpy(previousStage, stage, sizeof(previousStage) - 1);
        previousStage[sizeof(previousStage) - 1] = '\0';
        randomState = 0xD44B100Du ^ static_cast<u32>(room + 128);
        for (const char* it = stage; *it != '\0'; ++it) {
            randomState = randomState * 33u + static_cast<u8>(*it);
        }
        for (DarkHourBloodMark& mark : s_darkHourBloodPacket.marks) mark.active = false;
        s_darkHourBloodPacket.nextMark = 0;

        // Populate the complete loaded room in this first map frame. A
        // low-discrepancy disk covers distant geometry evenly instead of
        // relying on a small random circle around Link. Accepted marks remain
        // resident and are rendered regardless of their distance from Link.
        constexpr int MaxAttempts = 12000;
        constexpr int MaxMarks = 72;
        constexpr int MaxFloorLayersPerColumn = 8;
        constexpr f32 CoverageRadius = 40000.0f;
        constexpr f32 FloorSearchHeight = 30000.0f;
        constexpr f32 FloorSearchDepth = 30000.0f;
        const f32 diskRotation = dark_hour_blood_random_unit(randomState) * 6.2831853f;
        for (int attempt = 0;
             attempt < MaxAttempts && s_darkHourBloodPacket.nextMark < MaxMarks; ++attempt) {
            // 7919 is coprime with 12000, so this permutation visits the full
            // radius in a well-spread order rather than filling near Link first.
            const int diskSample = (attempt * 7919) % MaxAttempts;
            const f32 angle = diskRotation + diskSample * 2.39996323f;
            const f32 distance = sqrtf((diskSample + 0.5f) / MaxAttempts) * CoverageRadius;
            cXyz position(player->current.pos.x + sinf(angle) * distance,
                          player->current.pos.y + FloorSearchHeight,
                          player->current.pos.z + cosf(angle) * distance);

            // Walk down every collision layer in this X/Z column. The old
            // player-height restriction omitted upper stories, basements and
            // disconnected platforms even though their collision was loaded.
            for (int floorLayer = 0;
                 floorLayer < MaxFloorLayersPerColumn &&
                 position.y >= player->current.pos.y - FloorSearchDepth &&
                 s_darkHourBloodPacket.nextMark < MaxMarks;
                 ++floorLayer) {
                dBgS_GndChk groundCheck;
                groundCheck.SetPos(&position);
                const f32 ground = dComIfG_Bgsp().GroundCross(&groundCheck);
                if (ground == -G_CM3D_F_INF || ground < player->current.pos.y - FloorSearchDepth) {
                    break;
                }

                // Continue below this surface on the next pass even when it
                // is unsuitable, allowing a valid walkable floor beneath it.
                position.y = ground - 80.0f;

                cM3dGPla surface;
                if (!dComIfG_Bgsp().GetTriPla(groundCheck, &surface) ||
                    surface.mNormal.y < 0.78f) {
                    continue;
                }

                // Give each puddle a noticeably different footprint. The
                // larger range makes broad floor spills possible without
                // making every mark the same oversized shape.
                const f32 size = 220.0f + dark_hour_blood_random_unit(randomState) * 360.0f;
                const f32 rotation = dark_hour_blood_random_unit(randomState) * 6.2831853f;
                const f32 phaseA = dark_hour_blood_random_unit(randomState) * 6.2831853f;
                const f32 phaseB = dark_hour_blood_random_unit(randomState) * 6.2831853f;
                f32 roundedRadius[16];
                f32 extent = 0.0f;
                for (int sample = 0; sample < 16; ++sample) {
                    const f32 radiusAngle = sample * 0.39269908f;
                    // Low-frequency waves create broad organic curves with a
                    // slightly offset lobe, like a spill spreading across a
                    // floor. Avoid independent per-vertex noise so the edge
                    // stays soft instead of becoming star-shaped.
                    const f32 shape = 1.0f + 0.15f * sinf(radiusAngle * 2.0f + phaseA) +
                                      0.075f * sinf(radiusAngle * 3.0f + phaseB) +
                                      0.045f * sinf(radiusAngle + phaseA * 0.55f);
                    roundedRadius[sample] = size * shape;
                    if (roundedRadius[sample] > extent) extent = roundedRadius[sample];
                }

                cXyz floorPosition(position.x, ground, position.z);
                if (!dark_hour_blood_footprint_is_walkable(floorPosition, ground,
                                                           roundedRadius, rotation)) {
                    continue;
                }

                bool overlaps = false;
                for (u32 i = 0; i < s_darkHourBloodPacket.nextMark; ++i) {
                    const DarkHourBloodMark& existing = s_darkHourBloodPacket.marks[i];
                    const f32 dx = existing.position.x - floorPosition.x;
                    const f32 dz = existing.position.z - floorPosition.z;
                    const f32 separation = existing.extent + extent + 600.0f;
                    if (dx * dx + dz * dz < separation * separation &&
                        fabsf(existing.position.y - ground) < 120.0f) {
                        overlaps = true;
                        break;
                    }
                }
                if (overlaps) continue;

                DarkHourBloodMark& mark =
                    s_darkHourBloodPacket.marks[s_darkHourBloodPacket.nextMark++];
                mark.position = floorPosition;
                mark.position.y = ground + 0.8f;
                mark.surfaceNormal = surface.mNormal;
                mark.surfaceD = surface.mD;
                mark.extent = extent;
                mark.rotation = rotation;
                mark.sheenAngle = dark_hour_blood_random_unit(randomState) * 6.2831853f;
                for (int sample = 0; sample < 16; ++sample) {
                    mark.radius[sample] = roundedRadius[sample];
                }
                mark.active = true;
            }
        }
    }
}

}
void move() { dark_hour_blood_move(); }
void draw() {
    if (!dark_hour_moon_enabled() || g_env_light.camera_water_in_status != 0) return;
    dComIfGd_setXluListBG();
    j3dSys.getDrawBuffer(J3DSysDrawBuf_Xlu)->entryImm(&s_darkHourBloodPacket, 0);
    dComIfGd_setList();
}
}
