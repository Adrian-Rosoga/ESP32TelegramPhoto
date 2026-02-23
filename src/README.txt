TODO - High Prio:
- 12 Dec 2025 - Inspect code and fix to speed it up and make it more robust
- 12 Dec 2025 - Check why necessary to discard the first photo
- 12 Dec 2025 - Request photo via Telegram button

TODO - LowPrio:
- 12 Dec 2025 - Fix timestamp message if not NPT sync-ed.
If failed it outputs "Snap - Thursday 1970-01-01 00:01:03"

- HOWTO - OTA

See also AI-generated README_OTA.md

Note: Claude Opus 4.6 figured OTA out, GPT struggled in vain.

The partition file was important to setup.
The following was added to platformio.ini:
board_build.partitions = min_spiffs.csv

The partition file to which to point is, i.e.:
C:\Users\Adi\.platformio\packages\framework-arduinoespressif32\tools\partitions\min_spiffs.csv

Note:
Possibly the following set as admin in PowerShell is necessary:
PS C:\WINDOWS\system32> New-NetFirewallRule -DisplayName "Allow Python PlatformIO" -Direction Inbound -Program "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -Action Allow -Profile Private

1) Build the environments - esp32cam and esp32cam_ota

2) Ensure the IPs of the ESP32 and the computer from where one uploads are correct

Warning! Counterintuitive!
Below upload_port is the ESP32 IP address and upload_flags is the IP of the LG Gram laptop!

; Set this to your device IP or mDNS name (updated to your device IP)
upload_port = 192.168.1.196
; Pass the PC host IP to espota so the device can connect back to the uploader
upload_flags = --host_ip=192.168.1.131

3) Upload from command line:
pio run -t upload -e esp32cam_ota
