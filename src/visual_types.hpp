#pragma once
#include "dolphin/types.h"
// Private mod state, not part of the host ABI.
struct DuskTwilightAudioSequenceV1 {
    bool enabled;
    u8 style;
    bool sceneMusicForced;
    bool safeMusicEvent;
    bool mainReplacementReady;
    bool ordinaryBattle;
    bool battleFlagActive;
    bool subMusicEligible;
    bool replacementScene;
    bool customMusicEligible;
    bool battleScope;
    u8 musicMode;
    f32 gain;
    f32 battleVolume;
};
struct DuskTwilightRunningV1 {
    bool enabled;
    f32 speed;
    f32 heavyBootsRate;
};
struct DuskTwilightRenderPolicyV1 {
    bool astralFog;
    bool astralFragments;
};
struct DuskTwilightRgbV1 {
    u8 r;
    u8 g;
    u8 b;
};

struct DuskTwilightRgbaV1 {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
};

struct DuskTwilightSkyboxV1 {
    DuskTwilightRgbV1 sky;
    DuskTwilightRgbV1 cloudTop;
    DuskTwilightRgbV1 cloudBottom;
    DuskTwilightRgbaV1 cloudShadow;
    DuskTwilightRgbaV1 hazeOuter;
    DuskTwilightRgbaV1 hazeInner;
};
