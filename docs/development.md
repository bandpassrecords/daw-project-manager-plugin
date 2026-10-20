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

## The bridge driver

`DAWProjectManagerBridgeDriver` is one plugin instance's bridge logic
(`BridgeSession`) with no audio, GUI or host, driven over stdin/stdout. The
app's integration tests run it against the real app and a temp library, at a
plugin commit pinned in the app repo (`test/plugin_integration/plugin_ref`).

```
cmake -S . -B build -DDPM_BUILD_TESTS=OFF
cmake --build build --config Release --target DAWProjectManagerBridgeDriver
```

It lands in `build/DAWProjectManagerBridgeDriver_artefacts/<Config>/`. On a
fresh Ubuntu it needs only `pkg-config libfreetype-dev libfontconfig1-dev`
(for `juceaide`, which JUCE builds at configure time); CI's "Bridge driver on
Linux" job builds it with just those.

**Folder.** `DPM_BRIDGE_DIR`, an absolute path. Without it the driver prints
`{"ok":false,"error":…}` and exits 2: it never touches a real library.

**Protocol.** One JSON object per line on stdin, `{"cmd":"…", …args}`. Exactly
one line back per command: `{"ok":true, …}` or `{"ok":false,"error":"…"}`.
Nothing else goes to stdout. Before reading anything it prints
`{"ok":true,"ready":true,"runId":…,"version":…,"bridgeDir":…}`. It exits 0
after `quit` or at the end of input, without saying goodbye (send `goodbye`
first for a clean close; stopping without one is a crash, as the app sees it).

`REF` is `{"profileId","projectId","name"}`, or `null` when nothing is linked.

| Command | Arguments | Reply |
|---|---|---|
| `clock` | `now` (ISO 8601) or `advanceSeconds` | `now`. Real time until the first `clock`; fixed from then on |
| `host` | `name`, `windowTitle`? | |
| `settings` | `stampTodosWithTime`?, `captureMode`? (`off`/`automatic`/`manual`), `captureMaxSeconds`? | all three |
| `refresh` | | `changed`, `hasIndex`, `indexError` (string or null) |
| `autoLink` | | `changed`, `linked` (REF) |
| `linkTo` | `profileId`, `projectId`, `byUser`? (default true) | `linked` |
| `unlink` | | `linked` (null) |
| `addTodo` | `text`, `playheadSeconds`? (stamps only with `stampTodosWithTime`) | `added` |
| `setTodoCompleted` | `todoId`, `completed` | |
| `setNotes` | `text` | `changed` |
| `deliver` | | `written`, `undelivered` |
| `heartbeat` | | `heartbeatFile`, `present` |
| `goodbye`, `announceAgain` | | |
| `capture` | `seconds` (≤ 600), `sampleRate`? (48000), `channels`? (2), `end`? (`stopped`; also `reachedMax`, `silence`, `looped`, `offline`, `manualStop`, `modeChanged`, `none`), `manual`?, `silent`?, `overflowed`? | `verdict`, `seconds`, `file`, `relativeFile` (null unless kept) |
| `state` | | `runId`, `instanceId`, `linked`, `linkedByUser`, `hostName`, `todos` (`id`, `text`, `completed`, `pending`), `notes`, `appNotes`, `notesPending`, `undelivered`, `availableUpdate` (`version`, `url`, or null), `suggestions` (REFs), `lastCapture`, `stampTodosWithTime`, `captureMode`, `captureMaxSeconds` |
| `saveState` | | `state`: the XML a DAW saves with the project |
| `restore` | `state` | `linked`. The DAW reopening the project: says hello again |
| `quit` | | |

`capture` writes a 220 Hz tone (or silence) as a FLAC take, as the capture
engine does, and has the session judge it. `verdict` is `keep`,
`discardEnded`, `discardIncomplete`, `discardTooShort`, `discardSilent` or
`discardNotBetter`. An automatic take needs 10 s, a manual one 3 s.

`undelivered` counts the plugin's to-do and notes edits not yet in the outbox;
hellos and goodbyes are not counted.

## More

- [`PROTOCOL.md`](PROTOCOL.md): how the plugin and the app talk.
- [`CLAUDE.md`](../CLAUDE.md): how the code is organized, validating, and
  cutting a release.
