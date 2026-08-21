- Make it possible to join the device to an existing wifi; if its not available, switch to AP
- Built-in PlatformIO OTA (via pio run -t upload --upload-port IP_ADDRESS)
        Requires the device to have OTA bootloader support
        Your device already has OTA framework in place (src/ota_task.c)
        But PlatformIO's built-in OTA expects a specific protocol/bootloader integration
- Centralize hardware config (ADC channel, WiFi credentials, calibration defaults) into config.h
        Currently scattered across main.c, wifi_ap.c, signal_interpreter.c
- Version indication in app (under settings)

- Was wir jetzt außerdem noch machen müssen: ich möchte gerne die echte Lambdaspannung anzeigen und nicht die vom esp32 gemessene spannung (diese ist ja kompett irrelevant). deshalb musst du jetzt aus der gemessenen Spannung die echte spannung rückrechnen über den verstärkungsfaktor von 22/10 (2.2).  Überall in der app soll die korrekte lambdaspannung und nicht die gemessene angezeigt werden. mache die rückrechnung zentral am anfang der kette, sodass du nicht überall die spannungen separat umrechnen musst.  mach den faktor in den settings parametrierbar, sodass man ihn auch ändern könnte. alle einstellwerte in den settings bezüglich spannungen sollten dann auch in echten lambda-spannungen und nicht mehr die esp32 messspannungen eingestellt werden.