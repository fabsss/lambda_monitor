# lambda_monitor

## Firmware versioning

The Settings tab in the web UI reports the firmware version via ESP-IDF's
built-in app descriptor (`esp_app_get_description()`, exposed at
`GET /api/version` — see `src/web_server.c`), not a hand-maintained string.
That descriptor's `version` field is filled in at build time from
`git describe --always --tags --dirty`, so its accuracy depends entirely on
git tags being kept current.

**Rule: tag every release-worthy commit before or immediately after
flashing it**, using an annotated tag with semver:

```bash
git tag -a vX.Y.Z -m "Short description of the release"
git push origin vX.Y.Z
```

- Annotated (`-a`), not lightweight — `git describe` prefers annotated tags.
- Bump **patch** for bugfixes, **minor** for new features/backward-compatible
  changes, **major** for breaking changes (calibration format, API/URL
  changes, NVS schema changes).
- Don't tag mid-work or speculative commits — only commits that were
  actually built and flashed as a release.
- Never retag/move an existing tag to a different commit; cut a new one.

Do not reintroduce a separate hardcoded `FW_VERSION`-style macro — the app
descriptor is the single source of truth for what's actually running.

**Pitfall: incremental builds can embed a stale version.** ESP-IDF/CMake
only re-runs `git describe` at CMake *configure* time, not on every
incremental `pio run`. If you do several `pio run -e esp32s3` builds in a
row without a reconfigure (e.g. no CMakeLists.txt change), the embedded
version string can keep reflecting whatever commit/tag state existed at
the *first* configure of that build directory, even though the binary
itself was correctly recompiled from newer sources. Symptom: `/api/version`
reports a commit hash or tag that's older than what you just built.

**Rule: before flashing a release you're about to tag, force a clean
reconfigure** so the embedded version is trustworthy:

```bash
pio run -t clean -e esp32s3
pio run -e esp32s3
```

`ota_upload.py`'s "firmware is up to date, skipping build" mtime check is
a separate, correctly-working mechanism (it only skips the *compile* step
when no source/web/header file changed) — it does not protect against
this CMake-configure staleness, so don't rely on it alone before a
tagged release.
