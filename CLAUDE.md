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

## CI release builds

`.github/workflows/release.yml` triggers on any `v*.*.*` tag push: runs
`pio test -e test_native` as a gate, builds `esp32s3` from a fresh
checkout (always a clean configure, so no staleness risk there), verifies
the embedded version string matches the tag exactly (fails the build
otherwise — catches a dirty/mismatched tree before it ships), and attaches
`firmware.bin` + `bootloader.bin` + `partitions.bin` to a GitHub Release
via `softprops/action-gh-release`. Pushing a tag (`git push origin vX.Y.Z`)
is what triggers it — nothing else to do manually.

**`src/frontend_assets.h` is gitignored, not tracked.** It's generated
deterministically from `web/*` by `tools/embed_frontend.py`'s `pre:` build
step on every `pio run`. It used to be committed, which caused CI's
version-verify step to fail: a fresh checkout's committed copy could carry
a stale cache-hash comment (drifted out of sync from a `git add` that
happened before the embedded content was regenerated for the final commit),
so rebuilding it during CI touched exactly that one comment line and made
the tree look "dirty" to `git describe`, even with byte-identical embedded
HTML/JS. Do not re-add it to git — if you ever need to inspect the
generated output, it's on disk after any `pio run`, just untracked.

## Long-term stats persistence across firmware updates

`lambda_longterm_stats_t` (`lib/lambda_stats/lambda_stats.h`) is stored in
NVS and carries a `struct_version` (`LAMBDA_STATS_VERSION`) plus a CRC-32,
checked by `lambda_stats_validate()`. `nvs_store_load_stats()` no longer
falls straight back to a zeroed reset when that check fails on a version
bump — it first calls `lambda_stats_migrate_legacy()`, which recognizes
older on-disk layouts (by blob size, embedded version, and that version's
own CRC) and carries forward whatever fields the old layout had.

**Rule: whenever `LAMBDA_STATS_VERSION` is bumped, add the struct's
*previous* shape as a new `legacy_stats_vN_t` + migration case in
`lambda_stats.c`** (see the existing v1/v2/v3 cases) instead of accepting
that flashing the new firmware wipes the device's long-term stats. A field
that's genuinely new (no equivalent in the old layout) is left at its
`lambda_stats_reset()` default — only truly-unrecoverable data should be
lost, not everything.

`nvs_store_load_config()` (calibration) has no such version gate — it only
checks blob size, so it isn't affected by this and doesn't need migration
cases when `si_calibration_t` changes shape, since a size mismatch there
already falls back to `si_default_calibration()` safely (recalibration is
cheap; long-term stats are not).

## Wi-Fi AP connectivity-check responses must not claim internet access

`src/web_server.c`'s `/*` wildcard handler (`captive_portal_handler`)
answers every unmatched path — including the OS connectivity-check URLs
(`generate_204`, `hotspot-detect.html`, `connecttest.txt`, ...) — with a
`302` redirect to the device's own web UI, never a bare `204`/`200`.

**Pitfall: answering a connectivity check with "success" makes phones drop
mobile data.** This AP has no internet uplink. A prior version special-cased
`/generate_204` to return `204 No Content` specifically to silence
Android's "No Internet" notification — but that also makes the phone
believe this Wi-Fi network has full internet access, which can cause it to
deprioritize/disable mobile data while connected (reported: mobile data
turns off while parked/driving with the AP joined, unlike a real hotspot
with no internet, which correctly leaves mobile data active). Do not
reintroduce a per-path `204`/`200` "success" response for any connectivity-
check URL — the redirect is what tells the OS "no internet here, don't
route through this network," which is what keeps mobile data usable.

**The `302` redirect only matters if the client's DNS query for the
connectivity-check hostname actually reaches this device.** This AP has no
DNS relay of its own, so without `src/dns_hijack.c` a client's DNS query for
e.g. `connectivitycheck.gstatic.com` just times out — the check's HTTP
request never gets sent at all, and `captive_portal_handler` never gets a
chance to respond. `dns_hijack_start()` (called from `main.c` right after
`wifi_ap_start()`) runs a minimal UDP:53 server that answers every A-record
query with the AP's own IP (`192.168.4.1`), the same "DNS hijack" trick
WLED and ESP-IDF's own `captive_portal` example use — this is what actually
gets the OS's connectivity probe to this device's `httpd` in the first
place. Confirmed via a live comparison: a lambda_monitor AP without this
still shows "connected, no internet" on the phone (same as a real hotspot),
but *unlike* a real hotspot, mobile data still dropped — while WLED's AP
(which does DNS-hijack) left mobile data active. Losing this DNS server
silently reintroduces the mobile-data-drops-out symptom even though the
HTTP-level fix (above) is still in place, since the OS's probe request
never arrives to see it.

Also note `web_server_start()` must set `config.uri_match_fn =
httpd_uri_match_wildcard` on the `httpd_config_t` — `esp_http_server`'s
default matcher is a plain exact-string comparison, so without this the
`"/*"` entry for `captive_portal_handler` only ever matches a literal
request for `"/*"` and every unmatched path (including the connectivity-
check URLs once DNS-hijack gets them here) silently 404s instead of
getting the intended `302`.

**`CONFIG_HTTPD_MAX_REQ_HDR_LEN` must be raised well past ESP-IDF's 512-byte
default, or the connectivity check never reaches a handler at all.** A real
browser/OS HTTP client (Chrome, Android's own connectivity-check prober)
sends a full header set — `User-Agent`, `Sec-CH-UA-*`, `Accept-Language`,
etc. — that easily exceeds 512 bytes, well past what this project's own
lightweight `fetch()` calls in `app.js` ever send. Once exceeded,
`esp_http_server` rejects the request outright with a "header fields are
too long" error *before* it ever reaches `captive_portal_handler` — so the
DNS-hijack and wildcard-redirect fixes above can both be working perfectly
and the OS's connectivity probe still never sees the intended `302`
(confirmed by hitting `http://connectivitycheck.gstatic.com/generate_204`
directly in a phone browser while joined to the AP and getting exactly that
error page back). Set via `CONFIG_HTTPD_MAX_REQ_HDR_LEN=2048` in both
`sdkconfig.defaults` (so a from-scratch `sdkconfig` regeneration keeps it)
and directly in the committed `sdkconfig.esp32s3` (which, once it exists,
is what PlatformIO/ESP-IDF actually build from — `sdkconfig.defaults` only
seeds *new* keys or a from-scratch config, it does not override a value
already present in the frozen per-environment file).
