"""
PlatformIO post-build script for OTA upload via curl.
Automatically uploads after build if OTA_UPLOAD_HOST env var is set.
Usage: OTA_UPLOAD_HOST=192.168.4.1 pio run -e esp32s3
Or: OTA_UPLOAD_HOST=192.168.1.50 pio run -e esp32s3
Requires: curl installed and in PATH
"""

import subprocess
import sys
import os
from pathlib import Path

def upload_ota_post_build(source, target, env):
    """Upload firmware via OTA endpoint after build completes."""
    # Check if OTA upload is enabled via environment variable
    host = os.environ.get("OTA_UPLOAD_HOST")

    if not host:
        # OTA upload is optional - skip if not enabled
        return

    firmware_path = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    url = f"http://{host}/api/ota"

    if not Path(firmware_path).exists():
        print(f"ERROR: Firmware not found at {firmware_path}")
        sys.exit(1)

    print(f"\n{'='*70}")
    print(f"OTA Upload")
    print(f"{'='*70}")
    print(f"Target:   {url}")
    print(f"Firmware: {firmware_path}")
    print(f"{'='*70}\n")

    cmd = [
        "curl",
        "-X", "POST",
        "--data-binary", f"@{firmware_path}",
        url
    ]

    result = subprocess.run(cmd)

    if result.returncode != 0:
        print(f"\nERROR: OTA upload failed (curl exit code {result.returncode})")
        sys.exit(1)

    print(f"\n{'='*70}")
    print("✓ OTA upload complete! Device is rebooting with new firmware.")
    print(f"{'='*70}\n")

# Register as post-build action
Import("env")
env.AddPostAction("buildprog", upload_ota_post_build)
