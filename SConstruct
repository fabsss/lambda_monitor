"""
PlatformIO build customization.
Adds upload_ota task to the project tasks UI.
Usage: pio run -e esp32s3 -t upload_ota [--host=IP]
"""

import os
import subprocess
from pathlib import Path

Import("env")

def upload_ota_action(target, source, env):
    """Upload firmware via OTA to device."""
    # Get host from command line (--host=192.168.1.50) or default
    host = "192.168.4.1"

    for arg in ARGUMENTS.keys():
        if arg.startswith("host"):
            host = ARGUMENTS.get(arg)
            break

    firmware_path = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    url = f"http://{host}/api/ota"

    if not Path(firmware_path).exists():
        print(f"ERROR: Firmware not found at {firmware_path}")
        return 1

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
        return 1

    print(f"\n{'='*70}")
    print("✓ OTA upload complete! Device is rebooting with new firmware.")
    print(f"{'='*70}\n")
    return 0

# Register upload_ota as a custom target
env.AddCustomTarget(
    name="upload_ota",
    dependencies=["buildprog"],
    actions=[upload_ota_action],
    title="Upload OTA",
    description="Build and upload firmware via OTA to device (default: 192.168.4.1, use --host=IP for custom)"
)
