# CLAUDE.md — DAW Project Manager plugin

Instructions for AI assistants working on this codebase: the VST3/AU
plugin of [DAW Project Manager](https://github.com/bandpassrecords/daw-project-manager)
(issue #118). C++20, JUCE 9, CMake.

---

## Non-negotiables

### Tests are part of every task
- Every bug fix includes a regression test that would have caught it. Every
  new feature includes tests covering its logic in full.
- Run the whole suite before every commit; commit only when it passes:
  ```
  cmake --build build --config Debug --target DAWProjectManagerTests
  ctest --test-dir build -C Debug --output-on-failure
  ```
- CI runs the suite on Linux, Windows and macOS (`unit_tests` in
  `release.yml`), and every build waits for all three. Tests must hold on each:
  a juce::File can only hold a path native to the OS it runs on, so code that
  decides paths for another OS works on strings (`BridgePaths`).
- Logic goes in `src/core/`, which has no GUI, host or timer and is compiled
  into the tests. What is left in `src/plugin/` and `src/ui/` is glue: keep it
  thin enough not to need tests, and say so when something can't be automated.

### The protocol is a contract with the app
- `docs/PROTOCOL.md` is the contract; `BridgeProtocol`, `ProjectIndex`,
  `BridgeSession` and `Outbox` implement this side of it. Change them
  together, in the same commit, and the app side with them.
- Projects are matched by **profile and id, never id alone** — the app's rule
  (one id can sit in two profiles).
- The plugin never edits or deletes to-dos, never writes the app's files, and
  only ever deletes a capture it wrote itself and has since superseded.
- Add fields freely (both sides ignore unknown ones); a breaking change gets a
  new file name, not a new schema number.

### Never block the audio thread
- `processBlock` → `CaptureEngine::process` → `CapturePolicy::process`: no
  allocation, no locks, no file I/O, no logging. Samples cross to the writer
  thread through a lock-free FIFO. Use `prepareToWrite`/`finishedWrite`, never
  `AbstractFifo::write()`, whose scope object commits a short write of samples
  never copied.
- Audio passes through untouched. The test "The engine never changes the audio
  passing through" guards it.

### Always translate UI strings
- Every user-visible string goes through `TRANS("…")`, in English.
- The plugin ships in the app's nine languages: add the string to every file in
  `resources/translations/`. `LocalisationTests` fails on a missing or unused
  key and on a lost `{placeholder}`.
- `pt.txt` is **Brazilian Portuguese**, as in the app: "arquivo", "baixar",
  "tela", "usuário", "está gravando" — never "ficheiro", "descarregar", "ecrã",
  "utilizador", "está a gravar".
- Use the app's vocabulary for the same things (to-do = Aufgabe, tarea, tâche,
  attività, タスク, tarefa, задача, 待办事项).

### The name
- DAWs list the plugin as **DAW Project Manager** (by BandPass Records); its
  downloads are "DAW Project Manager Plugin". Don't call it "companion" in
  new names, text or docs.
- It was called DPM Companion until v0.1.x. These keep that name on purpose,
  because something outside this code depends on them — never change them:
  - the plugin codes (`Bnpr`/`Dpmc`) and the macOS bundle id
    `com.bandpassrecords.dpm.companion`: hosts find a saved plugin by them,
    and macOS installs by the bundle id;
  - `DPMCompanionState`, the saved state's tag in DAW projects;
  - the `companion` field of `index.json` and the app's
    `preview_songs/companion/`: the app's names, in the contract;
  - `{autopf}\DPM Companion`, where an update finds the uninstaller it
    replaces;
  - the old repository's release links (`isReleasePageUrl`), which older
    apps still send.
- The installers remove a `DPM Companion` bundle left from before, or the DAW
  would list the plugin twice.

### New state fields
- `PluginState` is saved in the DAW project (`getStateInformation`). A new
  field gets a default for older projects in `fromValueTree` and a round-trip
  check in `PluginStateTests`.
- Ask per field whether it belongs in the DAW project at all: a device-local
  preference doesn't (there are none yet).

---

## How it fits together

- `BridgeClient` (message thread, 2 s timer) drives one `BridgeSession`
  per instance: re-read `index.json`/`launch.json` when they change, read the
  host's window title, auto-link, deliver the outbox.
- Linking: `rankProjects` scores the window title (path > file name > name as
  a whole word, longer names first) and the app's launch hint; `confidentMatch`
  links only on a clear winner. A manual link (`linkedByUser`) is never changed
  automatically — the UI offers `relinkSuggestion` instead.
- To-dos: changes are `PendingTodoOp`s in the saved state until the index
  acknowledges them (`pruneAcknowledged`); `buildTodoList` lays them over the
  app's list. Undelivered ops are never dropped.
- Captures: `CapturePolicy` decides takes on the audio thread; `CaptureEngine`
  writes them as FLAC on its own thread into the temp folder; the session
  judges them (`judgeTake`) and moves keepers into `captures/`.
- Presence: after its hello an instance keeps `instances/<runId>.json` fresh
  (every 10 s, `BridgeClient`) and says goodbye when destroyed, relinked or
  unlinked (`BridgeSession::sayGoodbye`, `goodbyeTo`). The app ends work
  sessions on it, so: `runId` is per running instance and never saved (copies
  of a project share the saved instance id), and an instance that never said
  hello — a host's plug-in scan — never says goodbye.
- Stamped to-dos use the app's own format, `[MM:SS] text`
  (`playback_todo_utils.dart`). Keep them identical.
- UI colours are the app's Classic Dark (`palette` in `BrandLookAndFeel.h`),
  with the app icon's colours (`brand`) for accents: orange for the one main
  action of a section (`makePrimary`), navy for selection. The mark itself is
  `resources/brand/logo.svg`; `icon.png` and the installer's `icon.ico` are
  the app's own icon.

---

## Common tasks

### Building
```
cmake -S . -B build                         # downloads JUCE 9.0.4 and Catch2
cmake --build build --config Release --target DAWProjectManager_VST3
cmake -S . -B build -DDPM_BUILD_STANDALONE=ON   # a Standalone, to look at the UI
```
Pass `-DFETCHCONTENT_SOURCE_DIR_JUCE=<checkout>` to build against a local JUCE.
Set `DPM_BRIDGE_DIR` to a scratch folder when running the plugin or pluginval,
so nothing touches your real app library.

### Testing without a DAW: the Standalone and its test panel
`-DDPM_BUILD_STANDALONE=ON` builds `DAW Project Manager.exe`/`.app`, with a **test
panel** (`src/ui/TestPanel.*`) that only the Standalone shows
(`PluginProcessor::isStandalone`): a pretend DAW.
- **Transport:** Play/Stop, Rewind, Loop, and a test tone (`TestTransport`;
  JUCE mutes the Standalone's input).
- **Host:** the DAW's name and window title, for trying auto-linking.
- **Lifecycle:** Close project (goodbye), Reopen (hello), Crash DAW (silence,
  heartbeat left to go stale), Recover.

Run it with `DPM_BRIDGE_DIR` pointing at a test library's bridge folder, or at
none for the installed app's. CI attaches it to PR and manual builds, never to
a release. The panel is a developer tool: **English only, the one exception to
TRANS()**. Anything a user of the VST3/AU sees stays translated.

### Validating
```
pluginval --strictness-level 10 --validate-in-process --validate "build/DAWProjectManager_artefacts/Release/VST3/DAW Project Manager.vst3"
```
CI runs this on every platform, plus `auval` on macOS.

### Cutting a release
- The newest entry in `changelog/changelog.json` **is** the version; CMake
  reads it and CI fails a tag that doesn't match. Add it first:
  `python scripts/new_changelog_entry.py 0.2.0 --highlight "..."`.
- Then follow `.claude/skills/release/SKILL.md`: publish with
  `gh release create` (which creates the tag), never push a bare tag.

### Adding a UI string
1. `TRANS ("English text")` in the code.
2. The same key in every `resources/translations/*.txt`.
3. Run the tests.

---

## Key file locations

| What | Where |
|------|-------|
| The contract with the app | `docs/PROTOCOL.md` |
| Where the bridge folder is, per OS | `src/core/BridgePaths.*` |
| Message format, ISO times | `src/core/BridgeProtocol.*` |
| `index.json` / `launch.json` | `src/core/ProjectIndex.*` |
| Which project is open | `src/core/ProjectMatcher.*` |
| Everything one instance does with the bridge | `src/core/BridgeSession.*` |
| Saved state | `src/core/PluginState.*` |
| To-do merging and acknowledgement | `src/core/TodoBook.*` |
| Capture decisions / threading / FLAC | `src/core/CapturePolicy.*`, `CaptureEngine.*`, `CaptureWriter.*` |
| Host window title (Windows, macOS) | `src/platform/HostWindowTitle*` |
| Processor, timer glue | `src/plugin/` |
| Editor, picker, to-do list, look | `src/ui/` |
| Logo, icon | `resources/brand/` |
| Translations | `resources/translations/` |
| Installers | `packaging/` |
| CI and release | `.github/workflows/release.yml` |
