# Build Environment Record
**Date:** 2026-08-06  
**Firmware version built:** `2.1.26+47fde80-dirty (2026-08-06 20:45)`  
**Build result:** SUCCESS (Flash 61.4% used, RAM 17.4% used)

---

## Host Tools

| Tool | Version |
|------|---------|
| PlatformIO Core | 6.1.19 |
| Python | 3.13.2 |

---

## Platform

| Item | Value |
|------|-------|
| Name | pioarduino platform-espressif32 |
| Version | 53.03.10 (internal: 53.3.10) |
| platformio.ini setting | `platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.10/platform-espressif32.zip` |
| Download URL | https://github.com/pioarduino/platform-espressif32/releases/download/53.03.10/platform-espressif32.zip |
| Local install path | `%USERPROFILE%\.platformio\platforms\espressif32@src-3b1173dfe40c435bf3df49ae53f5cf59\` |

> **Note:** This is the [pioarduino](https://github.com/pioarduino/platform-espressif32) fork of the
> Espressif PlatformIO platform. It uses arduino-esp32 v3.x + ESP-IDF v5.x directly from Espressif
> GitHub releases, without going through the PlatformIO package registry CDN
> (`usc1.contabostorage.com`), which was unreachable at the time of this build.

---

## Packages Used in Build

All packages are installed under `%USERPROFILE%\.platformio\packages\`.

### framework-arduinoespressif32 @ 3.1.0
- **Description:** arduino-esp32 v3.1.0 (Arduino core for ESP32, ESP-IDF v5.3 based)
- **Owner:** espressif
- **Download URL:** https://github.com/espressif/arduino-esp32/releases/download/3.1.0/esp32-3.1.0.zip
- **Local path:** `framework-arduinoespressif32\` (no version suffix — this is the active slot)

### framework-arduinoespressif32-libs @ 5.3.0+sha.083aad99cf
- **Description:** Precompiled ESP-IDF v5.3 libraries for all ESP32 variants
- **Owner:** espressif
- **Download URL:** https://github.com/espressif/esp32-arduino-lib-builder/releases/download/idf-release_v5.3/esp32-arduino-libs-idf-release_v5.3-083aad99-v2.zip
- **Local path:** `framework-arduinoespressif32-libs\`

### toolchain-xtensa-esp-elf @ 13.2.0+20240530
- **Description:** GCC 13.2.0 cross-compiler for Xtensa ESP32
- **Owner:** platformio (registry ID 17066)
- **Download:** PlatformIO registry — `platformio/toolchain-xtensa-esp-elf@13.2.0+20240530`
- **Local path:** `toolchain-xtensa-esp-elf@13.2.0+20240530\`

### tool-esptoolpy @ 4.8.5
- **Description:** esptool.py for flashing/image creation
- **Owner:** pioarduino
- **Download URL:** https://github.com/pioarduino/esptool/releases/download/v4.8.5/esptool.zip
- **Local path:** `tool-esptoolpy@src-2ee4b59ebe071879a8d095e718e04167\`

### tool-mklittlefs @ 3.2.0
- **Description:** LittleFS filesystem image builder
- **Owner:** tasmota (registry ID 16072)
- **Download:** PlatformIO registry — `tasmota/tool-mklittlefs@3.2.0`
- **Local path:** `tool-mklittlefs\`

### tool-xtensa-esp-elf-gdb @ 14.2.0+20240403
- **Description:** GDB debugger for Xtensa ESP32 (optional, for debug sessions only)
- **Owner:** platformio (registry ID 17547)
- **Download:** PlatformIO registry — `platformio/tool-xtensa-esp-elf-gdb@14.2.0+20240403`
- **Local path:** `tool-xtensa-esp-elf-gdb\`

### tool-riscv32-esp-elf-gdb @ 14.2.0+20240403
- **Description:** GDB debugger for RISC-V ESP32 variants (optional, for debug sessions only)
- **Owner:** platformio (registry ID 17548)
- **Download:** PlatformIO registry — `platformio/tool-riscv32-esp-elf-gdb@14.2.0+20240403`
- **Local path:** `tool-riscv32-esp-elf-gdb\`

---

## Library Dependencies (from `lib_deps`)

Resolved by PlatformIO LDF and cached in `.pio/libdeps/esp32cam/`.

| Library | Version | Source |
|---------|---------|--------|
| UniversalTelegramBot | 1.3.0 | `witnessmenow/UniversalTelegramBot@^1.3.0` (PlatformIO registry) |
| ArduinoOTA | 3.1.0 | Bundled with framework-arduinoespressif32 @ 3.1.0 |
| WiFi | 3.1.0 | Bundled with framework-arduinoespressif32 @ 3.1.0 |
| NetworkClientSecure | 3.1.0 | Bundled with framework-arduinoespressif32 @ 3.1.0 (replaces WiFiClientSecure in v3.x) |

---

## platformio.ini Settings (esp32cam environment)

```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.10/platform-espressif32.zip
board = esp32cam
framework = arduino
extra_scripts = pre:version_build.py
monitor_speed = 115200
monitor_rts = 0
monitor_dtr = 0
board_build.arduino.upstream_packages = no
board_build.partitions = min_spiffs.csv
lib_deps = witnessmenow/UniversalTelegramBot@^1.3.0
```

---

## Background / Why This Configuration

The project was originally built with `platform-espressif32 @ v6.6.0` (from the PlatformIO GitHub).
That platform used packages distributed via the PlatformIO CDN mirror (`usc1.contabostorage.com`).

On 2026-08-06, the CDN became completely unreachable (DNS resolution failure for
`usc1.contabostorage.com`). This caused the build to fail in an infinite download-retry loop.
Additionally, the `variants/` directory had been deleted from the
`framework-arduinoespressif32@3.20014.231204` package, causing the original
`pins_arduino.h: No such file or directory` compile error.

The solution was to switch to the [pioarduino](https://github.com/pioarduino) fork of the platform,
which fetches all packages directly from Espressif GitHub releases and the PlatformIO registry
(not the dead CDN mirror). All packages happened to already be installed locally, so no downloads
were needed for the final successful build.

### .piopm metadata patches applied (workarounds for dead mirror)

Two package metadata files were patched to stop PlatformIO from re-downloading packages that were
already installed but had been replaced with different-but-compatible versions during the incident:

| Package | File | Original version | Patched to |
|---------|------|-----------------|------------|
| toolchain-xtensa-esp32 | `toolchain-xtensa-esp32\.piopm` | `8.4.0+2021r2-patch3` | `8.4.0+2021r2-patch5` |
| framework-arduinoespressif32 | `framework-arduinoespressif32\.piopm` | `3.1.0` | `3.20014.231204` |
| tool-esptoolpy | `tool-esptoolpy\.piopm` | `2.41100.0` | `1.40501.0` |

> These patches are no longer relevant with the pioarduino platform, which expects the actual
> installed versions. Do not rely on the patched values for the v6.6.0 platform.

---

## How to Reconstruct

1. Install PlatformIO Core 6.1.19 with Python 3.13.2
2. Use the `platformio.ini` in this repo as-is (pioarduino platform URL is pinned)
3. Run `pio run -e esp32cam` — PlatformIO will download all packages from the URLs above
4. All packages are either from Espressif GitHub releases or the PlatformIO registry (no
   dependency on the `usc1.contabostorage.com` CDN)
