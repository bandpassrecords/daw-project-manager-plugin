---
name: release
description: Cut a DAW Project Manager plugin release end to end. Writes the changelog entry (which is the plugin's version), verifies the CI version gate, publishes the GitHub release (which creates the tag and starts release.yml), watches the pipeline, and if it fails deletes the release and tag, diagnoses, fixes, commits, pushes and tries again until every build is green. Use when the user says "release", "cut a release", "release 0.2.0", "ship vX.Y.Z", or "publish a new version".
argument-hint: "[X.Y.Z]"
---

# Release

The same process as the app's release skill, minus the Flatpak metainfo.
Carries a release from "the code is on `main`" to "every asset is on the
GitHub release". Two checkpoints need the user, both at the start:

1. the version number, and
2. the notes: the changelog highlights plus the GitHub release notes.

Once the user has said go, keep going on your own through the pipeline,
failures and retries, reporting as you go.

**Repo:** `bandpassrecords/daw-project-manager-plugin` — pass `-R` to every `gh` command.

## How releasing works here (read first)

- **The GitHub release has to exist before the builds finish.** Each build
  job ends with `gh release upload` to the release of its tag and fails
  without one. So **never push a bare tag**: `gh release create` creates the
  tag on the remote, and that tag push starts the workflow.
- **One version gate**: the newest entry in `changelog/changelog.json` must
  match the tag. That entry is also the version CMake stamps into the plugin.
- **What a tag run builds:** unit tests on three OSes, then a Windows
  installer + zip, a macOS pkg + zip (signed and notarized when the secrets
  exist, `.github/REQUIRED_SECRETS.md`), and a Linux tarball. Each is
  validated with pluginval at strictness 10 (and auval on macOS) before
  packaging, and each asset gets a `.sha256`. It takes about **15 minutes**.
- **Retries are safe.** Nothing is published outside the GitHub release.
- **Use `gh` first** for anything on GitHub; `git` only for staging,
  committing, pushing and removing a local tag.
- **Commits:** follow the user's global rules. No `Co-Authored-By:` trailer
  and no "Generated with Claude Code" footer, in commits or release notes.

## 1. Version and pre-flight

- Version from `$ARGUMENTS` (strip a leading `v`), else: the newest remote tag
  (`gh api repos/bandpassrecords/daw-project-manager-plugin/git/matching-refs/tags/v --jq '.[].ref' | sort -V | tail -1`).
  If the changelog's newest entry is ahead of it, that is the version being
  released; otherwise propose a bump and ask.
- It must be `X.Y.Z`, higher than the newest tag, with no release or tag yet
  (`gh release view vX.Y.Z` and `gh api …/git/ref/tags/vX.Y.Z` both 404).
- Release from an up-to-date, clean `main` (or `release/vX.Y.Z` for a fix to
  a released version). If the work sits on an unmerged PR, stop and say so.

## 2. Notes (checkpoint: user approval)

- Collect the commits and merged PRs since the last tag (`gh api …/compare/<last-tag>...main`,
  `gh pr list --state merged`).
- Draft **changelog highlights** (one sentence per change a musician would
  notice; no refactors, CI or tests) and **GitHub release notes**
  (`### Highlights` with bold-lead bullets, `---`, then one `### <Area>` per
  theme). Show both; wait for OK.
- Write the entry: `python scripts/new_changelog_entry.py X.Y.Z --highlight "…"`
  (repeat `--highlight`; `--replace` amends; `--locale pt` etc. — `pt` is
  Brazilian Portuguese).

## 3. Verify and commit

- `git diff`: only `changelog/changelog.json` should have changed.
- Configure, build and run the whole suite (CLAUDE.md), and build the VST3:
  CMake reads the new version, so a typo in the entry fails here.
- Commit as `Prepare release vX.Y.Z`, push, and confirm GitHub has it
  (`gh api repos/bandpassrecords/daw-project-manager-plugin/commits/main --jq .sha` equals
  `git rev-parse HEAD`).

## 4. Publish

```
gh release create vX.Y.Z -R bandpassrecords/daw-project-manager-plugin \
  --target <sha of the pushed commit> --title "DAW Project Manager Plugin vX.Y.Z" --notes-file <notes file>
```

Find the run it started (`gh run list --workflow release.yml --branch vX.Y.Z --limit 1`);
accept only a run created after this attempt's release.

## 5. Watch

`gh run watch <run-id> --exit-status` in the background. On success, check
the assets: for each platform an installer or archive plus its `.sha256`
(Windows: `.exe` + `.zip`; macOS: `.pkg` + `.zip`; Linux: `.tar.gz`). A missing
asset is a failure. Report the release URL and the attempts it took.

## 6. When a run fails

1. `gh run view <run-id> --log-failed`: name the job and step, quote the error.
2. **Transient** (runner lost, download hiccup): `gh run rerun <run-id> --failed`,
   at most twice per job.
   **Needs the user** (secret missing or expired, certificate, Apple account):
   stop and report exactly what's needed; keep the release so a rerun finishes it.
   **A real problem** (test, gate, build, pluginval, workflow):
   `gh release delete vX.Y.Z --cleanup-tag --yes`, confirm the tag 404s,
   delete any local copy, fix on `main` with a regression test where it's a
   code bug, run the suite, commit the fix on its own, push, and go back to 4.
3. Stop after 3 attempts that each needed a code fix; report and ask.
