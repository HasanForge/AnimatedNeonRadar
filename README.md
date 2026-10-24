# Animated Neon Radar (GTA Vice City)

Standalone `plugin-sdk` ASI plugin that draws an animated "neon" radar in
place of the stock minimap. No money/health/armor/ammo/weapon-icon/star
code — radar only.

## Layout

```
AnimatedNeonRadar/
├── CMakeLists.txt
├── hud.ini                      # sample config, copy next to the game .exe
├── src/AnimatedNeonRadar.cpp    # the whole plugin
└── .github/workflows/build.yml  # CI: cross-compiles the .asi with MinGW
```

## Building

You need a local checkout of `https://github.com/dk22pac/plugin-sdk`
(either as a sibling `plugin-sdk/` folder next to this project, or point
`PLUGIN_SDK_DIR` at wherever you cloned it) and `i686-w64-mingw32-g++`.

```bash
git clone https://github.com/dk22pac/plugin-sdk
cmake -B build -S . \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DPLUGIN_SDK_DIR=./plugin-sdk
cmake --build build
```

Output: `build/AnimatedNeonRadar.asi`. Drop it into the VC install folder
next to an ASI loader, along with `hud.ini` and a `models/new_hud.txd`
containing textures named `Neon1`..`Neon120` and `Neon1a`..`Neon120a`.

### GitHub Actions

`.github/workflows/build.yml` does the same build on `ubuntu-latest`
(installs `mingw-w64`, clones `plugin-sdk` fresh each run, builds, and
uploads `AnimatedNeonRadar.asi` as a workflow artifact) — useful if you'd
rather not set up the MinGW toolchain locally.

## hud.ini keys

| Key                     | Default | Meaning                              |
|--------------------------|---------|---------------------------------------|
| `Custom Radar`           | `1`     | `1` = draw the neon radar, `0` = stock radar |
| `Custom Radar X Pos`     | `0.01`  | Left edge, fraction of screen width   |
| `Custom Radar Y Pos`     | `0.72`  | Top edge, fraction of screen height   |
| `Custom Radar Size X`    | `265.0` | Width in pixels                       |
| `Custom Radar Size Y`    | `220.0` | Height in pixels                      |
| `Animation Speed`        | `40`    | Milliseconds per frame                |

## Notes / things to double-check against your own plugin-sdk checkout

- `CMakeLists.txt` compiles plugin-sdk's `plugin_vc`, `shared`, `hooking`,
  and `safetyhook` sources straight into this target (same approach the
  SDK's own build uses for its "Plugin_VC" static lib, just in one pass).
  plugin-sdk is actively maintained and its internal layout/defines can
  shift between checkouts — if something doesn't compile, cross-check
  `tools/premake/premake5.lua` (functions `pluginSdkStaticLibProject` /
  `getExamplePluginDefines`) in your checkout for what changed.
- Memory addresses (`0xA0FD04`, `0x697C18`, `0x86963A`) are carried over
  unchanged from the original reference code and assume GTA:VC 1.0.
  
