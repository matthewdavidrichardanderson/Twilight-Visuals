# Twilight Visuals standalone module

## Ownership

The remaining feature implementations were extracted into the module:

- Settings/UI, palettes, weather, fog, grayscale, and post-processing.
- Grass texture/color conversion, Astral fragments, and Dark Hour blood puddles.
- Player running, iron-boot animation/rate, snow handling, water running,
  action-button behavior, and ledge boosts.
- Full-area environment-state acceptance and transition protection.
- Scene music selection, refresh state, battle classification, gain policy,
  MP3 decoding, looping/resampling, and fades.
- Authored sky archive loading, palette validation, and environment layer selection.
- Custom particle simulation/drawing, moon policy, and the monochrome background pass.

MFB retains callback registration/dispatch and borrowed engine access:
environment creation/update, actor/player and rendering callbacks, audio mixing,
sequence IDs, live audio-manager pointers, and existing frame interpolation.
Twilight-specific loaders, particle helpers, moon override storage, monochrome
rendering, music refresh state, and legacy visual configuration storage have
been removed from MFB. The module uses native game APIs through the SDK.
MFB does not discover or build this directory; build the mod independently through the SDK.

## Compatibility and installation

Use Twilight Visuals **1.2.0** with the matching MFB hook ABI revision **4**:
`build/windows-msvc/dusklight.exe` and
`build/windows-msvc/mods/twilight_visuals.dusk`.
An older AppData mod takes precedence over build-folder packages; update that
installed copy too. Mixing a new host and an old installed mod is unsupported.

The independent SDK build also produces
`build/twilight-visuals-standalone/mods/twilight_visuals.dusk`.
Revision 4 removes unused API slots and uses named sequence events. It is not
binary-compatible with revision 3; replace the installed mod alongside the host.
A separate mod build does not imply compatibility with older MFB releases.
The module rejects an incompatible hook contract; required hook installation
failures now fail initialization rather than silently losing features.

Music remains external beside the executable:

- Astral Plane.mp3
- Astral Plane CM.mp3
- tartarus 0d06.mp3
- Mass Destruction.mp3

No MP3 or converted music assets are packaged. User MP3s were not deleted.

## Verification

- Combined MFB executable and mod package built successfully.
- Independent SDK-based mod package built successfully.
- Host/module ABI declarations match.
- Fade unit test passed.
- All four user MP3s passed full decode, EOF, and rewind tests.
- Isolated F_SP121 save-load tests passed for styles 0, 1, 2, and 3;
  each stayed responsive and shut down gracefully with exit code 0.
- Installed AppData package hash matches the rebuilt package.
- Executable startup/help exited with code 0.
- Package content inspected: metadata and native module, no audio.
- Aurora was not modified.

These checks are **not full gameplay validation**.
Still visually test native Twilight, Faron/Lanayru transitions, all visual styles,
blood/fog, dungeon particles, running/iron boots/water, save-load music,
fanfares, combat transitions, MP3 EOF loops, speedrun restrictions,
and live mod disable/re-enable. The smoke tests do not confirm artistic sky
matching, audible combat/fanfare transitions, or every map.

Backup before deployment: `build/backups/twilight-hooks3-20260903-010823/`.
This includes the previous executable/PDB and previously installed mod.
