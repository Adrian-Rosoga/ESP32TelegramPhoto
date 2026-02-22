ESP32-CAM OTA (Over-The-Air) updates
===================================

This project now includes ArduinoOTA support so you can upload firmware over Wi‑Fi.

Quick steps
-----------

- (Optional) Set an OTA password: open `src/credentials.h` and set `OTA_PASSWORD` to a short password. Reflash once via USB after changing it.
- Flash the firmware once via USB (PlatformIO upload to `COMx`) so the OTA server is present on the device.
- Find the device IP or hostname in the Serial Monitor. The hostname is printed as `esp32cam-XXXXXX`.

Uploading using PlatformIO (examples)
-------------------------------------

- Upload to a device by IP (replace with your device IP):

```bash
pio run -t upload --upload-port 192.168.1.42
```

- Upload by hostname (if your OS resolves the mDNS hostname):

```bash
pio run -t upload --upload-port esp32cam-XXXXXX.local
```

Notes
-----
- If you set `OTA_PASSWORD` you must provide the same password when the uploader prompts for authentication (PlatformIO will pass the password automatically if the environment is configured).
- If upload fails, keep the device connected via USB and check the Serial Monitor for the IP/hostname and any OTA error messages.
- For the Arduino IDE: select the network port named `esp32cam-XXXXXX` (or the device IP) and upload normally.

If you want, I can add a PlatformIO `upload_port` environment or example `platformio.ini` snippet for automatic OTA uploads. 
