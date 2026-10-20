# Development

Needs CMake 3.22+ and a C++20 compiler (Visual Studio 2022, Xcode 15, GCC 11+);
on Linux, [JUCE's dependencies](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).
JUCE 9.0.4 and Catch2 are downloaded at configure time.

## Build

```
cmake -S . -B build
cmake --build build --config Release --target DAWProjectManager_VST3
```

The plugin lands in `build/DAWProjectManager_artefacts/Release/VST3/`.

## Test

```
cmake --build build --config Debug --target DAWProjectManagerTests
ctest --test-dir build -C Debug --output-on-failure
```

CI runs the same suite on Windows, macOS and Linux, and validates the plugin
with pluginval (and `auval` on macOS).

## Try it without a DAW

The Standalone app has a test panel that pretends to be the DAW: play/stop
with a test tone, loop, the DAW's window title, and closing, reopening or
crashing the project.

```
cmake -S . -B build -DDPM_BUILD_STANDALONE=ON
cmake --build build --config Debug --target DAWProjectManager_Standalone
```

Then run `build/DAWProjectManager_artefacts/Debug/Standalone/DAW Project Manager.exe`
(`.app` on macOS). Set `DPM_BRIDGE_DIR` to a test library's bridge folder
first, or it talks to your installed app's library:

```powershell
$env:DPM_BRIDGE_DIR = "C:\some\test\plugin_bridge"
& "build\DAWProjectManager_artefacts\Debug\Standalone\DAW Project Manager.exe"
```

PR and manual CI runs attach a ready-made Standalone for testers.

## More

- [`PROTOCOL.md`](PROTOCOL.md): how the plugin and the app talk.
- [`CLAUDE.md`](../CLAUDE.md): how the code is organised, validating, and
  cutting a release.
