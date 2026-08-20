"""
PlatformIO post-build script to enable OTA upload via curl.
Usage: pio run -e esp32s3 -t upload_ota -- --host 192.168.4.1
"""

import subprocess
import sys
from pathlib import Path

def upload_ota(target, source, env):
    """Upload firmware via OTA endpoint."""
    # Extract host from command-line args (default to 192.168.4.1)
    host = "192.168.4.1"

    # Check for --host argument
    for i, arg in enumerate(sys.argv):
        if arg == "--host" and i + 1 < len(sys.argv):
            host = sys.argv[i + 1]
            break

    firmware_path = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    url = f"http://{host}/api/ota"

    if not Path(firmware_path).exists():
        print(f"ERROR: Firmware not found at {firmware_path}")
        sys.exit(1)

    print(f"\n{'='*60}")
    print(f"Uploading firmware via OTA to {url}")
    print(f"Firmware: {firmware_path}")
    print(f"{'='*60}\n")

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

    print(f"\n{'='*60}")
    print("OTA upload complete! Device will reboot.")
    print(f"{'='*60}\n")

# Register as a post action
Import("env")
env.AddPostAction("buildprog", upload_ota)
env.AlwaysBuild(env.Command("upload_ota", None, upload_ota))
