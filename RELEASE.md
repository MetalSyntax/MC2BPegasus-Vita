# Release notes — MC2BPegasus-Vita (community fork)

This fork continues [WolffsRoom/MC2BPegasus-Vita](https://github.com/WolffsRoom/MC2BPegasus-Vita)
(port by **MeninoSung**), which has had no activity since August 2026. Changes below are
relative to upstream commit `253676d`.

## v1.25 — fork (unreleased)

### Intro movie (FFmpeg)
- The original `GloftBPHP/data/intro/logo.mp4` now plays as shipped. It is MPEG-4 Part 2
  854x480 @ 24 fps + AAC 48 kHz, which the Vita hardware decoder (H.264 only) cannot play.
- Replaced the SceAvPlayer path (`source/reimpl/intro_video.c`, which required a hand-made
  `logo-vita.mp4` and was otherwise skipped) with the software FFmpeg player
  `source/video.cpp`, integrated via `psvita-toolkit media integrate video-ffmpeg` from the
  Asphalt-6-Vita port (own MP4 demuxer + libavcodec MPEG-4/AAC, threaded decode, A/V sync on
  the audio clock).
- Cross or START skips the movie.

### Faster boot
- Log lines are batched in RAM (32 KB) instead of one `sceIoWrite` per line. Each write cost
  ~8–9 ms on retail hardware, adding several seconds to boot. Errors and explicit
  `port_trace_flush()` calls still reach the card immediately.
- vitaGL is built from `lib/vitagl` with `NO_SPLASHSCREEN`, `HAVE_SHADER_CACHE` and the
  speed-hack flags, so the 4 s startup serialization delay is gone and compiled shaders are
  cached in `ux0:data/moderncombat2/shader_cache/`.
- Fixed the boot crash (Data Abort in vitaGL `serialize_shader` on the first
  `glCompileShader`): it came from building with `DUMP_COMPILED_SHADERS=ON`, whose 32 KB dump
  buffer overflows. The option stays OFF; vitaGL's own cache replaces it.

### Android 1.0.5 support
- The loader detects the build at startup: 1.0.0 (Xperia Play, exports `m_isSlideUp`) uses
  render profile 4; 1.0.5 (generic touch) uses profile 2 and version string `1.0.5`, matching
  what each APK's Java code passes to `GameRenderer.nativeInit`.
- All JNI entry points, hooked symbols and the `Application`→`TouchScreenBase` offset (`+0x50`)
  were checked against both libraries.
- 1.0.5's `appOnKeyDown` is an empty stub, so on that build the left stick drives the
  on-screen movement stick through touch emulation (anchor `MOVE_STICK_X/Y` in
  `source/main.c`, still to be calibrated on hardware).

### Controls
- Circle no longer sends Android `BACK`, so it cannot open the pause menu mid-fight.
- START now sends `BACK` (pause / back in menus) on both builds.
- Full mapping table in the README.

### Menu without dead online services
- Gameloft LIVE!, More Games (IGP), Twitter and Multiplayer buttons are drawn at opacity 0:
  `gameswf::sprite_instance::add_display_object` is hooked and those instances get a colour
  transform with alpha 0 (`source/patch.c`).

### Diagnostics
- New per-launch log files `ux0:data/moderncombat2/logs/port_NNNN.log`.
- Every non-ATC compressed texture format and any upload vitaGL rejects is logged, to track
  down the white ground-weapon textures.

### Known limitations
- White ground weapons: not yet confirmed fixed on hardware (see README › Known Issues).
- 1.0.5: action buttons other than START are touch-only.
- Hidden online buttons can still be tapped.
