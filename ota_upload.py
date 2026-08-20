#!/usr/bin/env python3
"""
Simple OTA upload wrapper for lambda_monitor.
Usage:
    python ota_upload.py                    # Upload to 192.168.4.1 (default)
    python ota_upload.py 192.168.1.50       # Upload to custom IP
"""

import subprocess
import sys
from pathlib import Path

def main():
    host = "192.168.4.1"

    if len(sys.argv) > 1:
        host = sys.argv[1]

    # Build firmware
    print("Building firmware...")
    result = subprocess.run(["pio", "run", "-e", "esp32s3"])
    if result.returncode != 0:
        print("Build failed!")
        sys.exit(1)

    firmware_path = Path(".pio/build/esp32s3/firmware.bin")
    if not firmware_path.exists():
        print(f"Firmware not found: {firmware_path}")
        sys.exit(1)

    # Upload via OTA
    url = f"http://{host}/api/ota"
    print(f"\nUploading to {url}...")

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

    print("\n✓ Upload complete! Device is rebooting.")

if __name__ == "__main__":
    main()
