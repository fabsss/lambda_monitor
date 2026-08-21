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
