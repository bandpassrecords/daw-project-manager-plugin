# App ↔ plugin protocol

The DAW Project Manager app and the DPM Companion plugin never talk directly.
They exchange files in one shared folder, the **bridge folder**:

- the app tells the plugin what it knows (projects and their to-dos);
- the plugin leaves messages for the app (new to-dos, ticks, captures).

Files were chosen over a socket on purpose. The app may be closed while you
work in the DAW, and the plugin must keep working then: changes wait in the
outbox, or in the DAW project itself, until the app runs. The plugin stays
simple, and every linking decision stays in the app, as issue #118 proposed.

`src/core/BridgeProtocol.*`, `ProjectIndex.*` and `CompanionSession.*` implement
the plugin side. **This file and that code change together.**

## Where the bridge folder is

It sits inside the app's own data directory, so it follows the installed
app's library:

| OS | Path |
|---|---|
| Windows | `%LOCALAPPDATA%\daw_project_manager\plugin_bridge` |
| macOS | `~/Library/Application Support/com.bandpassrecords.dpm/plugin_bridge` |
| Linux | `$XDG_DATA_HOME/com.bandpassrecords.dpm/plugin_bridge` (default `~/.local/share/…`) |
| Linux, Flatpak only | `~/.var/app/com.bandpassrecords.dpm/data/com.bandpassrecords.dpm/plugin_bridge` |

On Linux the plugin picks the Flatpak's folder only when the native app's
data folder doesn't exist.

The environment variable `DPM_BRIDGE_DIR` (absolute path) points the plugin
somewhere else. Use it for development builds of the app, which keep their
library under another name (`app_paths.dart`), and for tests and CI.

The app should resolve the same folder with `getLocalAppDataPath()` +
`plugin_bridge`. In a pinned or dev library that path differs from the
release one. That is correct: a PR build must not talk to plugins working
for the stable app.

```
plugin_bridge/
├── index.json          written by the app
├── launch.json         written by the app
├── outbox/             written by the plugin, emptied by the app
│   └── 20261018T120000123Z-000003-1a2b3c4d.json
└── captures/           written by the plugin, emptied by the app
    └── Teardrop Remix_20261018-140233.flac
```

Every file is written to a temporary name and renamed into place, so a reader
never sees half a file:

- **Plugin side:** an outbox message is written as `….tmp` and renamed to
  `….json`. A capture is recorded outside the bridge folder and moved in
  whole; across volumes it is copied as `.<name>.part` first.
- **App side:** write `index.json.tmp`, then rename it over `index.json`. Do
  the same for `launch.json`.

Readers skip anything that isn't `*.json` in `outbox/`, and anything starting
with `.` or ending in `.part` in `captures/`.

All times are ISO 8601. Writers use UTC with a `Z` suffix
(`DateTime.toUtc().toIso8601String()`). Readers accept fractions of any length
and explicit offsets.

## Written by the app

### `index.json`

What every plugin instance knows about the library. Rewrite it whenever
something it contains changes: a project added, renamed, moved or deleted, a
to-do changed, and **after applying outbox messages** (that is how the plugin
learns they arrived, see [Acknowledgement](#acknowledgement)).

```json
{
  "schema": 1,
  "generatedAt": "2026-10-18T12:00:00.000Z",
  "app": { "version": "2.10.0" },
  "projects": [
    {
      "id": "6f0c…",
      "profileId": "a1b2…",
      "profileName": "Audio Crawler",
      "name": "Teardrop (Audio Crawler Remix)",
      "filePath": "C:\\Music\\Remixes\\Teardrop (Audio Crawler Remix).cpr",
      "dawType": "Cubase",
      "todos": [
        { "id": "9d4e…", "text": "Tighten the kick", "completed": false,
          "createdAt": "2026-10-01T09:00:00.000Z" }
      ]
    }
  ]
}
```

- **Every profile's projects**, not just the active profile's. A DAW session
  belongs to whichever artist it belongs to. Each entry names its own
  `profileId`, and the plugin matches a project by **profile and id**, never by
  id alone (one id can sit in two profiles).
- **Projects with a file only.** Leave out version stacks, which are virtual
  rows with no file, and archived projects. A stack's **members** are listed
  as usual, each carrying **the stack's to-dos**: a stack owns the shared
  to-dos, and the plugin never needs to know stacks exist.
- `name` is the name the app shows. `filePath` is the project file. `dawType`
  is `MetadataExtractor`'s name (`"Cubase"`, `"Ableton Live"`, `"Reaper"`, …).
- `todos` keep the app's order. The plugin shows open to-dos first.
- `companion` (optional) names the newest plugin release:
  `{ "latestVersion": "0.2.0", "releaseUrl": "https://github.com/bandpassrecords/dpm-companion/releases/tag/v0.2.0" }`.
  The app looks it up on GitHub (the plugin itself never goes on the network)
  at most once a day, only when the user's update checks are on and only in
  builds where the app checks its own updates. It leaves the field out
  otherwise. The plugin shows "Update available" when that version is newer
  than its own, and opens the link only if it points at the plugin's releases
  page.
- `schema` is `1`. Fields may be added freely: the plugin ignores what it
  doesn't know. A breaking change gets a new file name (`index.v2.json`), not a
  new schema number in the same file.

### `launch.json`

Written just before `launchProjectInDaw` opens a project. It helps the
plugin link without asking where the DAW's window title can't be read (Linux)
or says nothing useful.

```json
{ "projectId": "6f0c…", "profileId": "a1b2…",
  "filePath": "C:\\Music\\…\\Teardrop.cpr", "launchedAt": "2026-10-18T12:00:00.000Z" }
```

A launch less than 15 minutes old links on its own. Up to 12 hours old, it is
only a suggestion. A window title naming another project always wins.

## Written by the plugin: `outbox/`

One message per file. **Apply them in file-name order** (names start with
the UTC time and a sequence number), then delete each file once it is
applied. Every message has the same envelope:

```json
{
  "schema": 1,
  "id": "c1d2e3f4-…",
  "type": "todo.add",
  "createdAt": "2026-10-18T12:00:05.000Z",
  "plugin": { "version": "0.1.0", "instanceId": "…" },
  "host": { "name": "Steinberg Cubase 14", "windowTitle": "Cubase Pro Project - Teardrop" },
  "project": { "id": "6f0c…", "profileId": "a1b2…", "name": "Teardrop", "filePath": "C:\\…\\Teardrop.cpr" },
  "payload": { }
}
```

- `project` is `null` when the plugin isn't linked. Only `capture.ready` is
  ever sent unlinked. An app could match it by `host.windowTitle`, the way the
  plugin does; the app currently drops it, along with its capture.
- `id` is unique per message. Applying the same message twice must change
  nothing: remember applied ids, or make each apply idempotent as below.
- A message of a `type` the app doesn't know is logged and deleted. A newer
  plugin must not be able to fill the folder.

### `session.hello`

A plugin instance is running inside this project: just linked, or loaded
with a saved link when the DAW opened the project. Payload:
`{ "linkedByUser": true }`. Use it as you like, for example to note that
the project is open or to offer starting its session timer. It needs no
reply.

### `todo.add`

```json
"payload": { "todoId": "8a7b…", "text": "[01:23] Vocal too loud", "createdAt": "2026-10-18T12:00:05.000Z" }
```

Add a `TodoItem` with **`id = todoId`** (the plugin made it, a UUID v4),
`text`, `createdAt`, `completed: false`. For a stack member, add it to the
stack. If a to-do with that id already exists, do nothing: that is what makes
a replay harmless.

A text starting with `[MM:SS]` (or `[HH:MM:SS]`) was stamped with the
playhead time. It is the same text the app's own "Add to-do at timestamp"
writes (`buildTimestampedTodoText`).

### `todo.setCompleted`

```json
"payload": { "todoId": "9d4e…", "completed": true, "changedAt": "2026-10-18T12:00:09.000Z" }
```

Set that to-do's `completed`. If the to-do is gone (deleted in the app), drop
the message: a deletion beats a tick.

### `capture.ready`

```json
"payload": {
  "file": "captures/Teardrop_20261018-140233.flac",
  "supersedes": "captures/Teardrop_20261018-131500.flac",
  "durationSeconds": 60.0,
  "sampleRate": 48000,
  "channels": 2,
  "peakDbfs": -1.2,
  "trigger": "auto",
  "capturedAt": "2026-10-18T14:02:33.000Z"
}
```

A pre-listen recording of the project, 24-bit FLAC, `file` relative to the
bridge folder. The app (`companion_bridge_service.dart`):

1. copies it to `preview_songs/companion/<project id>/`;
2. makes it the project's preview song (the stack's, for a member), but only
   when the project has no preview, its preview is an earlier capture, or its
   preview is an auto-detected mixdown older than the capture. A preview the
   user picked is never replaced;
3. deletes the file in `captures/`, adopted or not.

Rules:

- **The file may be gone.** The plugin deletes the capture it superseded when
  a better one arrives, which can happen before the app ever ran. A
  `capture.ready` whose file is missing has been superseded: skip it.
- `supersedes` names the capture this one replaces, if the plugin made one
  before. If the app adopted that one, this one replaces it.
- `trigger` is `"auto"` (a playback pass) or `"manual"` (the Record button).
- The plugin never records loops being cycled or offline renders: the app's
  mixdown detection already covers bounces.

## Acknowledgement

The plugin keeps every to-do change it made in its saved state (inside the
DAW project) until the index shows it:

| Change | Acknowledged when |
|---|---|
| `todo.add` | the project's `todos` contain `todoId` |
| `todo.setCompleted` | an index with `generatedAt` after the change shows that `completed`, or the to-do is no longer listed |

So **after applying messages, rewrite `index.json` with a fresh
`generatedAt`.** Until then the plugin shows those to-dos with a "waiting for
the app" clock. Each message is written once and never resent; until it is
confirmed, the change stays visible in the plugin and saved with the project.
If the outbox can't be written at all, the change waits in the saved state and
is written the next time it can be.

## What the plugin never does

- It never deletes or edits to-do text.
- It never writes `index.json` or `launch.json`.
- It never deletes outbox messages: that is the app's job once it has applied
  them.
- In `captures/` it deletes only a capture it wrote itself and has since
  superseded.
