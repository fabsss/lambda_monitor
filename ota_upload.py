#!/usr/bin/env python3
"""
Simple OTA upload wrapper for lambda_monitor.
Only rebuilds if source files changed. Uses PlatformIO's incremental build.

Usage:
    python ota_upload.py                    # Upload to 192.168.4.1 (default)
    python ota_upload.py 192.168.1.50       # Upload to custom IP
    python ota_upload.py --force             # Force rebuild
"""

import subprocess
import sys
from pathlib import Path
import os

def main():
    host = "192.168.4.1"
    force_build = False

    # Parse arguments
    for arg in sys.argv[1:]:
        if arg == "--force":
            force_build = True
        else:
            host = arg

    firmware_path = Path(".pio/build/esp32s3/firmware.bin")

    # Check if rebuild is needed
    if not force_build and firmware_path.exists():
        # Check if any source files are newer than the firmware
        firmware_mtime = firmware_path.stat().st_mtime
        src_dir = Path("src")
        web_dir = Path("web")

        rebuild_needed = False

        # Check source files
        for src_file in src_dir.glob("**/*.[ch]"):
            if src_file.stat().st_mtime > firmware_mtime:
                rebuild_needed = True
                print(f"Source changed: {src_file}")
                break

        # Check web files
        if not rebuild_needed:
            for web_file in web_dir.glob("**/*"):
                if web_file.is_file() and web_file.stat().st_mtime > firmware_mtime:
                    rebuild_needed = True
                    print(f"Web file changed: {web_file}")
                    break

        # Check header files
        if not rebuild_needed:
            include_dir = Path("include")
            if include_dir.exists():
                for hdr_file in include_dir.glob("**/*.h"):
                    if hdr_file.stat().st_mtime > firmware_mtime:
                        rebuild_needed = True
                        print(f"Header changed: {hdr_file}")
                        break

        if not rebuild_needed:
            print(f"✓ Firmware is up to date, skipping build")
        else:
            print("Rebuilding firmware...")
            result = subprocess.run(["pio", "run", "-e", "esp32s3"])
            if result.returncode != 0:
                print("Build failed!")
                sys.exit(1)
    else:
        if force_build:
            print("Building firmware (forced)...")
        else:
            print("Building firmware...")
        result = subprocess.run(["pio", "run", "-e", "esp32s3"])
        if result.returncode != 0:
            print("Build failed!")
            sys.exit(1)

    if not firmware_path.exists():
        print(f"Firmware not found: {firmware_path}")
        sys.exit(1)

    # Upload via OTA
    url = f"http://{host}/api/ota"
    print(f"\n{'='*70}")
    print(f"OTA Upload")
    print(f"{'='*70}")
    print(f"Target:   {url}")
    print(f"Firmware: {firmware_path} ({firmware_path.stat().st_size / 1024:.1f} KB)")
    print(f"{'='*70}\n")

    cmd = [
        "curl",
        "-X", "POST",
        "--data-binary", f"@{firmware_path}",
        url
    ]

    result = subprocess.run(cmd)

    if result.returncode != 0:
        print(f"Upload failed!")
        sys.exit(1)

    print(f"\n{'='*70}")
    print("✓ Upload complete! Device is rebooting with new firmware.")
    print(f"{'='*70}\n")

if __name__ == "__main__":
    main()
