<p align="center">
  <img src="assets/PSVita_Screen.png" alt="Modern Combat 2: Black Pegasus - PS Vita Port">
</p>

# MC2BPegasus-Vita

Unofficial wrapper/port of **Modern Combat 2: Black Pegasus** for PlayStation Vita.

The port operates by loading the official Android ARMv7 executable directly into memory, linking its dependencies to native functions, providing the JNI environment expected by the game, and translating OpenGL ES calls through vitaGL. In practice, this creates a lightweight Android-like environment where the original executable can run natively on the PS Vita.

> Port by **MeninoSung** · Community fork maintained by **Wonder Diaz**

> [!NOTE]
> **About this fork.** The original repository ([WolffsRoom/MC2BPegasus-Vita](https://github.com/WolffsRoom/MC2BPegasus-Vita)) has had no activity for months, so this fork continues the work: FFmpeg playback of the original intro movie, faster boot, Android **1.0.5** support, reworked controls and a menu without the dead online services. Every change is listed in [RELEASE.md](RELEASE.md). All credit for the original port stays with its author.

## About the port

This project uses an Android shared-object loader and FalsoJNI to run the original `libsandstorm2.so` library on the PS Vita. The graphics layer is implemented with [VitaGL](https://github.com/Rinnegatamante/vitaGL), while platform-specific replacements provide input, audio, filesystem, lifecycle, and media functionality expected by the Android version.

The target game build uses the Gameloft package namespace `com.gameloft.android.GAND.GloftBPHP.ML`, the Xperia Play device profile, and game data stored under `GloftBPHP/data/`. The loader reproduces the JNI initialization sequence used by that build instead of relying on a generic `android_main` entry point.

_This port does not distribute the game's commercial data. Users must provide their own legally obtained, compatible Android copy and extract the required files themselves._

## Setup Instructions (For End Users)

In order to properly install the game, follow these steps precisely:

- Install [kubridge](https://github.com/TheOfficialFloW/kubridge/releases/) and [FdFix](https://github.com/TheOfficialFloW/FdFix/releases/) by copying `kubridge.skprx` and `fd_fix.skprx` to your taiHEN plugins folder (usually `ur0:tai`) and adding the following entries to `config.txt` under `*KERNEL`:

```text
*KERNEL
ur0:tai/kubridge.skprx
ur0:tai/fd_fix.skprx
```

**Note:** Do not install `fd_fix.skprx` if you are using the rePatch plugin.

- **Optional:** Install [PSVshell](https://github.com/Electry/PSVshell/releases/) if you want to monitor performance. Overclocking is not required: the port runs at a stable 60 FPS at the PS Vita's standard 333 MHz CPU clock.
- Install `libshacccg.suprx`, if you do not already have it, by following [this guide](https://samilops2.gitbook.io/vita-troubleshooting-guide/shader-compiler/extract-libshacccg.suprx).
- Obtain a legal copy of **Modern Combat 2: Black Pegasus** for Android ARMv7, version **1.0.0** or **1.0.5**.
- Do not combine `libsandstorm2.so` and data files from different releases.

### Supported Android Builds

| Property | 1.0.0 (Xperia Play) | 1.0.5 (generic touch) |
|---|---|---|
| Native library | `libsandstorm2.so` (6.4 MB) | `libsandstorm2.so` (7.9 MB) |
| Package namespace | `com.gameloft.android.GAND.GloftBPHP.ML` | same |
| Render profile sent to the engine | 4 (Sony) | 2 (generic) |
| Texture format in `3d.pak` | ATC (decoded to RGBA8 by the loader) | PVRTC 2bpp (native on the Vita GPU) |
| Physical buttons | Native (Xperia Play key path) | Engine ignores key presses; sticks are mapped to touch (see Controls) |

The loader detects the build at startup (the 1.0.0 Xperia library exports `m_isSlideUp`, 1.0.5 does not) and picks the profile and version string automatically. Only `libStormGLOFT.so` from the 1.0.5 APK is not needed.

### How to Prepare the Game Files

1. Extract your legally obtained Android game package and game data (1.0.0 **or** 1.0.5).
2. Locate `libsandstorm2.so` in the APK's ARMv7 library folder.
3. Locate the matching `GloftBPHP/data` directory.
4. Create a folder named `moderncombat2` with the following structure:

   ```text
   Moderncombat2/
   |-- libsandstorm2.so
   |-- files/
   `-- GloftBPHP/
       |-- CheckPoint.bin
       |-- SaveGame.bin
       `-- data/
           |-- Constants.bin
           |-- 2d.header
           |-- 2d.pak
           |-- 3d.header
           |-- 3d.pak
           |-- automat.header
           |-- automat.pak
           |-- structs.header
           |-- structs.pak
           |-- texts.header
           |-- texts.pak
           |-- audio/
           `-- intro/
               `-- logo.mp4
   ```

5. Keep all files from the same release together. Do not replace individual files with data from another version.
6. Keep `intro/logo.mp4` exactly as it ships. The port decodes the original MPEG-4 Part 2 + AAC movie in software with FFmpeg; no conversion or `logo-vita.mp4` is needed anymore.

### Installation on PS Vita

1. Install `MC2BPegasus-Vita.vpk` on your PS Vita using VitaShell.
2. Copy the prepared `moderncombat2` folder to `ux0:data/`.
3. Verify that the files are located at these paths:

   ```text
   ux0:data/moderncombat2/libsandstorm2.so
   ux0:data/moderncombat2/GloftBPHP/data/
   ```

4. Launch **PegasusVita** from the LiveArea.

If startup fails, check the newest `ux0:data/moderncombat2/logs/port_NNNN.log` (one file per launch). The loader also creates `ux0:data/mc2_boot.log` when the application reaches its main entry point.

### Language

The supported languages are:

- English
- French
- German
- Italian
- Spanish
- Japanese

For the best compatibility, set the game to English after the first launch:

```text
Options > Languages > English
```

## Controls

The port supports the front touchscreen, both analog sticks, and the physical PS Vita controls. On 1.0.0 the physical buttons reach the game through its original Xperia Play key path. **Circle no longer opens the pause menu**; START is now pause / back.

| PS Vita control | Sent to the game as | 1.0.0 (Xperia Play build) | 1.0.5 (touch build) |
|---|---|---|---|
| Left analog stick | D-pad keys (1.0.0) / touch drag on the movement stick (1.0.5) | Move | Move (touch emulation) |
| Right analog stick | Touch drag on the right half of the screen | Look / aim | Look / aim |
| D-pad | `DPAD_UP/DOWN/LEFT/RIGHT` | Move / menu navigation | — (engine ignores keys) |
| Cross | `DPAD_CENTER` | Confirm / Xperia action | — |
| Circle | _unbound_ | Free (no longer opens the menu) | Free |
| Square | `BUTTON_X` | Xperia Play X action | — |
| Triangle | `BUTTON_Y` | Xperia Play Y action | — |
| L | `BUTTON_L1` | Xperia Play L1 | — |
| R | `BUTTON_R1` | Xperia Play R1 | — |
| START | `BACK` | Pause / back in menus | Pause / back in menus |
| SELECT | `BUTTON_SELECT` | Xperia Play Select | — |
| Front touchscreen | Touch events | Menus and on-screen controls | Full HUD (fire, grenade, reload, crouch, pause…) |
| Cross / START during the intro | — | Skip intro movie | Skip intro movie |

_The exact in-game action of each Xperia Play button can vary by screen and control configuration._

## Screenshots

<p align="center">
  <img src="assets/prints/1.png" width="48%" alt="Modern Combat 2 gameplay screenshot 1">
  <img src="assets/prints/2.png" width="48%" alt="Modern Combat 2 gameplay screenshot 2">
  <img src="assets/prints/3.png" width="48%" alt="Modern Combat 2 gameplay screenshot 3">
  <img src="assets/prints/4.png" width="48%" alt="Modern Combat 2 gameplay screenshot 4">
  <img src="assets/prints/3-FPS.png" width="48%" alt="Modern Combat 2 running at 60 FPS on PS Vita">
</p>

The game runs at a stable **60 FPS** on standard PS Vita clocks, including the default **333 MHz CPU clock**. Overclocking is not required for normal gameplay.

## Build Instructions (For Developers)

Requirements:

- [VitaSDK](https://vitasdk.org/) configured through the `VITASDK` environment variable;
- an ARM `softfp` toolchain and compatible dependencies;
- vitaGL (built from `lib/vitagl` with the flags in `CMakeLists.txt`), vitashark, FalsoJNI, OpenSLES, and the audio libraries referenced by `CMakeLists.txt`;
- FFmpeg for the intro movie: `vdpm ffmpeg` and `vdpm lame` once;
- `kubridge.skprx` installed on the target Vita.

### Compile

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The current repository includes the distributable as `MC2BPegasus-Vita.vpk`. A local CMake build generates `Pegasus_Vita.vpk` with title ID `MC2V00001`. By default, the loader expects the game files at `ux0:data/moderncombat2/`. The path can be changed at configure time:

```bash
cmake -S . -B build \
  -DDATA_PATH="ux0:data/moderncombat2/" \
  -DGAME_SO_PATH="ux0:data/moderncombat2/libsandstorm2.so"
cmake --build build
```

Leave `DUMP_COMPILED_SHADERS` **OFF** (the default). vitaGL already caches compiled shaders in `ux0:data/moderncombat2/shader_cache/`, and the dump path overflowed its fixed 32 KB buffer on the first shader (Data Abort in `serialize_shader`).

Debug builds enable additional loader diagnostics:

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

## Known Issues

- Touch input still has some interaction bugs caused by conflicts with the virtual movement joystick.
- Weapons lying on the ground can render white with 1.0.0 (ATC) data. 1.0.5 data uses PVRTC, which the Vita GPU decodes natively; the loader now logs every compressed format and any rejected upload (`GL: native compressed…` / `GL: compressed upload rejected…`) so the remaining cases can be pinned down. Not yet confirmed on hardware.
- On 1.0.5 the engine discards physical key presses (`appOnKeyDown` is an empty stub in that build). START (pause/back) and both sticks work; fire, reload, grenade and the other actions use the touchscreen. The left-stick touch anchor (`MOVE_STICK_X/Y` in `source/main.c`) may need calibration on hardware.
- Online services (Gameloft LIVE!, More Games, Twitter, multiplayer) no longer exist. Their main-menu buttons are drawn at opacity 0 but their area can still be tapped.

## Legal Notice

This is an unofficial, free, non-commercial port. **Modern Combat 2: Black Pegasus**, Modern Combat, Gameloft, and all related assets and trademarks belong to their respective developers and copyright holders.

This repository must not be used to distribute commercial APKs, native libraries, or game data. Use only files from a copy you obtained legally and support the original developers.

## Credits

The loader code is derived from work by Volodymyr Atamanenko and the wider PS Vita Android-porting community. FalsoJNI is distributed in `lib/falso_jni` under its own license. Rights to Modern Combat 2 and its assets belong to their respective owners.

- **Port by MeninoSung**
- TheFloW for the original Android `.so` loading research and infrastructure.
- Volodymyr Atamanenko (`v-atamanenko`) for the loader, FalsoJNI-related work, and Gameloft porting references.
- Rinnegatamante for [VitaGL](https://github.com/Rinnegatamante/vitaGL).
- VitaSDK contributors and the PS Vita homebrew community.
- Original game by Gameloft and its respective rights holders.

---

## AI Notice

Artificial intelligence tools (Codex/ChatGPT) were used to assist with code analysis, compatibility investigation, documentation, and the iterative development process.
