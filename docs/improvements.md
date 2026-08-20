- Make it possible to join the device to an existing wifi
- Built-in PlatformIO OTA (via pio run -t upload --upload-port IP_ADDRESS)
        Requires the device to have OTA bootloader support
        Your device already has OTA framework in place (src/ota_task.c)
        But PlatformIO's built-in OTA expects a specific protocol/bootloader integration
- Centralize hardware config (ADC channel, WiFi credentials, calibration defaults) into config.h
        Currently scattered across main.c, wifi_ap.c, signal_interpreter.c
        Move ADC channel to NVS so it can be changed at runtime via web UI (with reboot)
        Add /api/hardware endpoint to configure ADC channel without reflashing