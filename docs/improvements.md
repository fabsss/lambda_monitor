- Make it possible to join the device to an existing wifi; if its not available, switch to AP
- Built-in PlatformIO OTA (via pio run -t upload --upload-port IP_ADDRESS)
        Requires the device to have OTA bootloader support
        Your device already has OTA framework in place (src/ota_task.c)
        But PlatformIO's built-in OTA expects a specific protocol/bootloader integration
- Centralize hardware config (ADC channel, WiFi credentials, calibration defaults) into config.h
        Currently scattered across main.c, wifi_ap.c, signal_interpreter.c
- Version indication in app (under settings)