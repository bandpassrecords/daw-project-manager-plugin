# DPM Companion

The DAW plugin for [DAW Project Manager](https://github.com/bandpassrecords/daw-project-manager).
Put it on your master bus and it:

- **records a pre-listen of the project while you work.** Each playback pass
  becomes the project's preview song in DAW Project Manager, without
  bouncing. That covers projects that were never bounced, and bounces that
  have fallen months behind the project (issue #118).
- **shows the project's to-dos inside the DAW.** Tick them off, add new ones,
  and optionally stamp a new one with the playhead time
  (`[01:42] vocal too loud`), in the same format the app's player uses.
- **knows which project it's in.** It reads the DAW's window title (Cubase,
  Live, FL Studio, REAPER, Studio One, …) and notices when the app has just
  launched a project. Otherwise you pick the project from a search. The link
  is saved with the DAW project.

Audio passes through untouched.

VST3 on Windows, macOS and Linux, plus AU on macOS. Available in the app's nine
languages.

## How it talks to the app

Through files in a folder inside the app's data directory. The app writes the
list of projects and their to-dos; the plugin leaves messages and recordings
for the app to pick up. Nothing is lost while the app is closed: to-dos wait
in the outbox, or in the DAW project itself, until the app runs.
[`docs/PROTOCOL.md`](docs/PROTOCOL.md) is the full contract.

## Install

Download from the [releases](https://github.com/bandpassrecords/dpm-companion/releases):

| | |
|---|---|
| Windows | `DPM_Companion_Installer_vX.Y.Z.exe` (installs to `C:\Program Files\Common Files\VST3`) or the `.zip` |
| macOS | `DPM_Companion_macOS_Installer_vX.Y.Z.pkg` (VST3 + AU) or the `.zip` |
| Linux | `DPM_Companion_Linux_x86_64_vX.Y.Z.tar.gz`, then `./install.sh` |

Each file has a `.sha256` next to it.

## Using it

1. Insert **DPM Companion** on the master bus.
2. Check the **Project** card. It should name your project; if not, press
   **Change…** and pick it.
3. **Pre-listen capture**:
   - *Automatic* records each playback pass, from the first sound, up to the
     length you choose. It skips loops being cycled and offline renders
     (bounces are found by the app anyway). A new pass replaces the capture
     when it is at least as long.
   - *Manual* records between **Record** and **Stop**.
4. **To-dos**: click one to tick it. Type in the field and press Enter to
   add one. A clock icon means the app hasn't confirmed it yet.

## Build

Needs CMake 3.22+ and a C++20 compiler (Visual Studio 2022, Xcode 15, GCC 11+);
on Linux, [JUCE's dependencies](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).

```
cmake -S . -B build
cmake --build build --config Release --target DPMCompanion_VST3
cmake --build build --config Debug --target DPMCompanionTests
ctest --test-dir build -C Debug --output-on-failure
```

JUCE 9.0.4 and Catch2 are downloaded at configure time. See
[`CLAUDE.md`](CLAUDE.md) for how the code is organised, and how to validate and
release.

## Licence

AGPL-3.0, the licence JUCE is available under for open-source projects (see
[LICENSE](LICENSE)). The plugin is a separate program from DAW Project Manager
(MIT): the two communicate only through files.
