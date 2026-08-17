# :framed_picture: CYD Photo Frame :pager:
[![Coveralls](https://img.shields.io/badge/dynamic/xml?url=https%3A%2F%2Fcoveralls.io%2Frepos%2Fgithub%2Fnicholaswilde%2Fcyd-photo-frame%2Fbadge.svg%3Fbranch%3Dmain&query=%2F%2F*%5Blocal-name()%3D'text'%5D%5Blast()%5D&label=Coveralls&style=for-the-badge&logo=coveralls)](https://coveralls.io/github/nicholaswilde/cyd-photo-frame?branch=main)
[![task](https://img.shields.io/badge/Task-Enabled-brightgreen?style=for-the-badge&logo=task&logoColor=white)](https://taskfile.dev/#/)
[![test](https://img.shields.io/github/actions/workflow/status/nicholaswilde/cyd-photo-frame/test.yaml?label=test&style=for-the-badge&branch=main&logo=github-actions)](https://github.com/nicholaswilde/cyd-photo-frame/actions/workflows/test.yaml)
[![ci](https://img.shields.io/github/actions/workflow/status/nicholaswilde/cyd-photo-frame/ci.yaml?label=ci&style=for-the-badge&logo=github-actions)](https://github.com/nicholaswilde/cyd-photo-frame/actions/workflows/ci.yaml)

A digital photo frame for the ESP32 Cheap Yellow Device (CYD) 

> [!WARNING]
> This project is currently in a `v0.X.X` development stage. Features and configurations are subject to change, and breaking changes may be introduced at any time.

## :hammer_and_wrench: Hardware Requirements

- **ESP32 Cheap Yellow Device (CYD)**:
  - **CYD 2.8" (Resistive)**: ESP32-2432S028R — 2.8″ 320×240 ILI9341 LCD with XPT2046 resistive touch.
  - **CYD 3.5" (Capacitive)**: ESP32-3248S035C — 3.5″ 480×320 ST7796 LCD with GT911/CST820 capacitive touch.
- **Storage**: MicroSD card slot (SDHC up to 32 GB out-of-the-box; SDXC up to 2 TB supported when reformatted to FAT32).
- Micro-USB / USB-C cable for power and programming.

> [!WARNING]
> Due to SPI bus timing and hardware characteristics on the CYD board, the SD card may intermittently fail to mount on boot. Resetting or power cycling the device usually resolves the issue. See [#15](https://github.com/nicholaswilde/cyd-photo-frame/issues/15) for more details.

## :star: Features

- **High-Performance Caching:** Converts JPEGs to raw RGB565 images on boot, enabling under **60ms** rendering times.
- **Catppuccin Theme Flavors:** Fully dynamic Settings panel and slideshow background borders in Mocha, Macchiato, Frappé, or Latte.
- **Auto & Percentage-Based Brightness:** Easily control LCD backlight and onboard RGB LED brightness using percentage scales (10–100% for LCD, 0–100% for LED) across web and on-device UI, with optional room LDR light sensor auto-adjustment.
- **Wi-Fi & Captive Portal Manager:** Toggleable Wi-Fi support with automated Access Point fallback (`cyd-photo-frame-<mac>`) and Captive Portal configuration webpage (`192.168.4.1`) for setting network credentials. Includes dynamic connection status icon in the Settings panel header which opens a detailed Wi-Fi Info modal (SSID, IP, MAC address, RSSI) when tapped.
- **Remote HTTP File Upload:** Access a beautiful Catppuccin-themed webpage at `http://<device-ip>/upload` to manage files wirelessly! Supports drag-and-drop, multi-file sequential upload queues, queue removal, and on-demand device restarting to immediately render new photos. When uploading while on the "No Photos" screen, the device displays an active on-screen upload progress bar and status indicator without interrupting slideshow playback.
- **Over-The-Air (OTA) Firmware Updates:** Flash new firmware wirelessly directly from your browser at `http://<device-ip>/update` or via the web dashboard.
- **Dynamic Multi-Device Support:** Auto-detects device hardware targets (`ESP32-2432S028R` 2.8" or `ESP32-3248S035C` 3.5") across web interface headers and footers.
- **Unified Screen Design:** Consistent header typography, Catppuccin color palette, and structured vertical label layout across all system screens (Settings, Optimization, Wi-Fi Setup, SD Card Errors, Warnings, and confirmation modals).
- **Filename Banner Overlay:** Displays a clean, toggleable Catppuccin Mantle banner containing the current image name at the bottom of the screen.
- **Touch Navigation Zones:** Easily navigate images and access settings by tapping designated screen areas.
- **On-Screen Feedback Banners:** Displays a temporary top toast notification banner to confirm touch zone settings changes on-screen (e.g. brightness, interval, random mode, etc.) before auto-restoring the photo.
- **Settings Storage Management:** Clear the on-device photo cache directly from the Settings menu with an interactive confirmation prompt and visual progress bar tracking cache folder deletion.
- **Interactive Warning Screens:** Displays informative warning screens (such as "No Photos Found") equipped with a direct **⚙️ Settings** button to adjust device settings or configure Wi-Fi without rebooting.
- **REST API & Web Server Controls:** Read and update full device configurations via `/api/config` JSON endpoints, with the ability to toggle API server access on or off in the Settings panel.
- **MQTT & Home Assistant Auto-Discovery:** Full integration with Home Assistant via MQTT Discovery, exposing controls for LCD backlight, auto-brightness, random mode, filename banner overlay, inactivity sleep, bypass optimization, boot from cache, slideshow interval, theme flavor, screen orientation, playback controls (next, previous, pause/resume), and diagnostics (Wi-Fi RSSI, IP, MAC address, uptime, free heap, current image filename, and total images).

## :world_map: Touch Navigation Zones

The screen is divided into a 3x3 touch grid to control slideshow behavior and parameters directly:

| Zone | Action |
| --- | --- |
| **Middle-Left** | Show **previous** photo |
| **Middle-Right** | Show **next** photo |
| **Middle-Center (Tap)** | **Pause / Resume** slideshow |
| **Middle-Center (Long Press, 1.5s)** | **Open Settings Menu** |
| **Top-Left** | Increase backlight brightness |
| **Bottom-Left** | Decrease backlight brightness |
| **Top-Center** | Toggle filename banner display |
| **Bottom-Center** | Toggle random slideshow mode |
| **Top-Right** | Increase slideshow interval (+1s) |
| **Bottom-Right** | Decrease slideshow interval (-1s) |

## :floppy_disk: SD Card Configuration

> [!WARNING]
> **Auto-Formatting Warning:**
> On boot, if the SD card fails to mount (e.g., due to partition corruption or being raw/unformatted), the firmware is configured to automatically format the card to **FAT32**. 
> If you insert a corrupted card or experience connection issues, the card's contents **will be wiped**. Always keep backup copies of your images elsewhere.

1. Format your MicroSD card to **FAT32**.
   - **SDHC Cards (up to 32 GB):** Supported out-of-the-box when formatted as FAT32.
   - **SDXC Cards (64 GB, 128 GB, 256 GB+):** Supported on both CYD 2.8" and CYD 3.5", but **must be reformatted to FAT32** (e.g., using `FAT32 Format`, `guiformat`, or `mkfs.fat -F 32`). Standard `exFAT` or `NTFS` formats are **not supported** by the ESP32 `SD` library. Maximum FAT32 volume limit is **2 TB** (max single file size is **4 GB**).
2. Copy your images to the SD card:
   - **JPEGs (`.jpg`):** Place directly in the root directory (`/`) of the SD card.
   - **Raw RGB565 images (`.raw`):** Must be stored in the `/cache/` folder on the SD card (e.g. `/cache/image_320x240.raw`).
3. Plug the card into the CYD SD slot. On boot, the ESP32 will auto-detect any new JPEGs, scale them (downscaling larger images and upscaling smaller images) to fit the screen while keeping their aspect ratios, and cache them inside the `/cache/` directory.

> [!WARNING]
> **On-Device Optimization Speed & Bypass Mode:**
> On-device JPEG scaling and caching on the ESP32 takes significant time (~1 minute per photo for high-resolution images from modern phone cameras; e.g., processing 235 photos on a CYD 2.8" device took ~3 hours 49 minutes). 
> 
> You can **bypass this on-device optimization phase entirely** by toggling **Bypass Optimization** to **ON** in the Settings menu. When enabled, the device will skip boot-time scaling calculations and progress screens, booting directly into the slideshow. It is recommended to use the `scripts/prepare_images.py` script with the `--raw` flag to pre-generate and save raw RGB565 files directly into the SD card's `/cache` folder.

> [!NOTE]
> **Display Orientation & Caching:**
> The device now supports four orientations (Landscape, Portrait, Landscape Rev, Portrait Rev).
> Cached images are stored per resolution (e.g., `_320x240.raw` for landscape modes and `_240x320.raw` for portrait modes), so switching between orientations no longer requires clearing the cache. The cache can be manually cleared via the Settings menu or serial command, or automatically cleared upon theme flavor changes. 

## :signal_strength: Wi-Fi Setup & Captive Portal

1. Open the **Settings** menu by long-pressing the center of the screen for 1.5 seconds.
2. Toggle the **WiFi** switch to **ON**, then tap **Save & Exit**. The device will save your preference and reboot into Wi-Fi mode.
3. If no network credentials are configured yet (or if the saved network is out of range), the frame enters **Access Point (AP) Mode** and displays the setup screen on the TFT display prior to running photo calculation and optimization:
   - **AP SSID:** `cyd-photo-frame-<mac>` *(where `<mac>` is the last 4 digits of the MAC address)*
   - **IP Address:** `192.168.4.1`
4. Connect your phone or computer to the frame's Wi-Fi Access Point (`cyd-photo-frame-XXXX`).
5. A captive portal page will pop up automatically (or navigate to `http://192.168.4.1/`), allowing you to scan local Wi-Fi networks and save your SSID and password.
6. Upon saving, credentials persist to NVS and the device reboots to connect to your network.
7. Wi-Fi status is displayed in real-time in the top-right header of the Settings menu:
   - **Green Icon:** Connected to Wi-Fi. Tapping the icon opens an interactive **Wi-Fi Info** screen displaying current network SSID, IP address, MAC address, and signal strength (RSSI).
   - **Yellow Icon:** Connecting or AP Mode active.
   - **Red Icon:** Disconnected or connection failed.

> [!NOTE]
> **Improv Wi-Fi Provisioning**
> The device supports [Improv Wi-Fi](https://www.improv-wifi.com/) provisioning over Serial. Please note that Improv is only active and available to detect your device when **WiFi is toggled ON** in the Settings menu.

## :cloud: Remote HTTP File Upload

The CYD Photo Frame hosts a built-in web server to allow wireless media management from your computer or smartphone when connected to Wi-Fi.

1. Turn on Wi-Fi in the Settings panel and note the **IP Address** assigned to the frame (tap the green Wi-Fi icon in the top right to view).
2. Open a web browser on any device on the same local network and navigate to: `http://<frame-ip-address>/upload`.
3. You will be greeted with a beautiful Catppuccin-themed drag-and-drop interface.
4. **Drag & Drop** multiple image files into the dashed area, or click to open your file browser.
5. The files are added to a **pending queue**. You can review the selected files and click the **"X"** button to remove any unwanted files.
6. Click **Upload All** to begin a sequential background upload directly to the device's SD Card. (If the device is currently on the "No Photos" screen, it will display a live progress bar tracking each file upload in real time.)
7. Once all uploads show as `Done` and the queue is finished, a **Restart Device** button will automatically appear. Click it to remotely reboot the frame, which forces it to rescan the SD card and instantly include your new photos in the slideshow!

## :arrows_counterclockwise: Over-The-Air (OTA) Firmware Updates

Update device firmware wirelessly without needing a USB cable.

1. Ensure Wi-Fi is enabled and connected on your CYD Photo Frame.
2. Open a web browser and navigate to `http://<frame-ip-address>/update` (or click **Firmware Update** from the web dashboard at `http://<frame-ip-address>/`).
3. Drag & drop your compiled `.bin` firmware file into the update page (or click to browse).
4. Click **Update Firmware**. A progress bar will track the upload.
5. Upon completion, the device automatically reboots into the new firmware version.

## :camera: Screenshots

### Device UI Showcase

| Screen | Landscape (320x240) |
|---|---|
| **Settings** | ![Settings](screenshots/current_settings.png) |
| **Access Point Mode** | ![AP Mode](screenshots/current_ap.png) |
| **Photo Optimization** | ![Optimization](screenshots/current_opt.png) |
| **Loading Slideshow** | ![Loading](screenshots/current_loading.png) |
| **Resuming Slideshow** | ![Resuming](screenshots/current_resuming.png) |
| **Clear Cache Prompt** | ![Clear Cache](screenshots/current_clear_cache.png) |
| **No Photos Warning** | ![Warning](screenshots/current_warn.png) |
| **SD Card Error** | ![SD Error](screenshots/current_sd_error.png) |

The device supports capturing the current screen as a standard 24-bit BMP image via HTTP or serial commands.

### Remote HTTP API & Screen Capture

> [!NOTE]
> Wi-Fi must be enabled and connected. The device IP is printed to serial on boot or displayed in the Settings screen Wi-Fi info modal.

**Capture Screenshot via HTTP:**
```bash
# Save directly to file
curl http://<CYD_DEVICE_IP>/screenshot -o screenshot.bmp

# Or run via Taskfile (requires CYD_DEVICE_IP in .env)
task screenshots
```

### REST API Settings Management

You can read and update the device configuration via the `/api/config` JSON REST API endpoint.

**GET `/api/config`**

Example response:
```json
{
  "brightness": 50,
  "auto_brightness": true,
  "slideshow_interval": 15,
  "random_mode": false,
  "show_filename": true,
  "inactivity_sleep": false,
  "theme_flavor": 0,
  "screen_orientation": 0,
  "led_brightness": 10,
  "led_enabled": true,
  "wifi_enabled": true,
  "api_server_enabled": true,
  "mqtt_enabled": false,
  "wifi_ssid": "Your_SSID",
  "wifi_password": "Your_Password",
  "bypass_opt": false,
  "boot_cache": false,
  "mqtt_server": "",
  "mqtt_port": 1883,
  "mqtt_user": "",
  "mqtt_password": "",
  "ap_password": "",
  "mqtt_base_topic": "cyd/photo_frame/",
  "static_ip_enabled": false,
  "static_ip": "",
  "static_gateway": "",
  "static_subnet": "",
  "static_dns": ""
}
```

**POST `/api/config`**

Send a JSON payload with any combination of the fields above to update the configuration. The device will apply the settings and return `{"status":"ok"}`.

### Serial Commands

If the CYD is plugged into your computer via USB:
1. Open the PlatformIO Serial Device Monitor (at `115200` baud).
2. Press **`Ctrl + T`** then **`Ctrl + E`** to enable local line editing/input mode.
3. Type **`clear`** or **`clear_cache`** and press **`Enter`** to clear the image cache.
4. Type **`screenshot`** and press **`Enter`** to capture the LVGL Settings screen (saved as BMP on the SD card).
5. Type **`screenshot_tft`** and press **`Enter`** to capture the current raw TFT screen (e.g., Optimization) as BMP on the SD card.
6. The ESP32 will format/empty the `/cache` directory (for clear) or write the BMP file, then automatically reboot if the cache was cleared.

## :rocket: Getting Started

### 1. Quick Install (Pre-compiled Binaries)

**Option A: Web Flasher (Easiest)**

You can easily flash the pre-compiled firmware directly from your browser using the [ESP32 Web Flasher](https://nicholaswilde.io/esp32-web-flasher/). This requires a Web Serial compatible browser (like Chrome or Edge).

**Option B: Terminal Script**

You can flash the device directly from your terminal using the provided flash script. Replace `/dev/ttyUSB0` with your actual serial port. By default, it flashes the `cyd_28r` version, but you can specify the device as the first argument.

```bash
# Flash the default cyd_28r
bash -c "$(curl -fsSL https://raw.githubusercontent.com/nicholaswilde/cyd-photo-frame/main/scripts/flash.sh)" _ cyd_28r /dev/ttyUSB0

# Or flash the cyd_35c version
bash -c "$(curl -fsSL https://raw.githubusercontent.com/nicholaswilde/cyd-photo-frame/main/scripts/flash.sh)" _ cyd_35c /dev/ttyUSB0
```

> [!WARNING]
> Running a script directly from the internet with `bash -c "$(curl...)"` is a potential security risk. Always review the script's source code before executing it to ensure it is safe. You can view the script [here](https://github.com/nicholaswilde/cyd-photo-frame/blob/main/scripts/flash.sh).

*(For more detailed flashing instructions, you can reference the [frame-fi flashing guide](https://nicholaswilde.io/frame-fi/flashing-firmware/)).*

### 2. Build from Source

#### Environment Setup (`.env` and `config/secrets.h`)

Initialize local environment configuration by copying the templates:

```bash
task init
# Or manually:
# cp .env.example .env
# cp config/secrets.h.example config/secrets.h
```

#### Configurable Variables in `.env`:
| Variable | Default | Description |
|---|---|---|
| `PIO_ENV` | `cyd_28r` | Default PlatformIO target environment (`cyd_28r` or `cyd_35c`) |
| `CYD_DEVICE_IP` | `192.168.1.100` | Local network IP address of the CYD device (used for `task screenshots`) |
| `COVERALLS_REPO_TOKEN` | *(optional)* | Token for uploading coverage reports to Coveralls (`task coverage:upload`) |

#### MQTT & Home Assistant Configuration
MQTT can be configured either dynamically via the web settings portal (`http://<device-ip>/`) or compiled into default fallback firmware values via `config/secrets.h`:
```cpp
#define MQTT_SERVER   "192.168.1.100" // Your broker IP or hostname
#define MQTT_PORT     1883
#define MQTT_USER     "my_mqtt_username"
#define MQTT_PASSWORD "my_secure_password"
```

When MQTT is enabled:
- **Home Assistant Auto-Discovery:** Devices register automatically under the prefix `homeassistant/` with entities for LCD Brightness, Auto Brightness, Random Mode, Show Filename, Inactivity Sleep, Bypass Optimization, Boot from Cache, Slideshow Interval, Theme Flavor, Screen Orientation, Playback controls (Next, Previous, Pause/Play), and Restart.
- **Sensors & Diagnostics:** Reports device connection status, uptime, free heap memory, Wi-Fi RSSI, IP, MAC address, firmware version, current photo filename, and total images count.
- **Decoupled Publishing Cadence:**
  - **Dynamic Diagnostics & Telemetry:** Sensor and diagnostic topics (`system/uptime`, `system/free_heap`, `system/wifi_rssi`, `system/ip`, `system/version`, `system/mac`, and `state/image`) publish every 60 seconds.
  - **Operational Settings:** Device settings topics (`settings/brightness`, `settings/auto_brightness`, `settings/random_mode`, `settings/show_filename`, `settings/inactivity_sleep`, `settings/bypass_optimization`, `settings/boot_from_cache`, `settings/api_server_enabled`, `settings/slideshow_interval`, `settings/theme`, `settings/screen_orientation`) publish on a 10-minute heartbeat interval, or immediately when modified.
- **State & Command Topics:** Default base topic is `cyd/photo_frame/` (e.g. `cyd/photo_frame/state/image`, `cyd/photo_frame/command/slideshow_interval`, `cyd/photo_frame/command/theme`, etc.).

#### Build & Upload
Select the environment matching your hardware:

```bash
# For CYD 2.8" (Resistive Touch, ILI9341)
pio run -e cyd_28r -t upload
# (Use cyd_28r_inv if your colors are inverted)

# For CYD 3.5" (Capacitive Touch, ST7796)
pio run -e cyd_35c -t upload
# (Use cyd_35c_inv if your colors are inverted)

# Start the Serial Monitor
pio device monitor
```

---

## :framed_picture: Preparing Images

A helper script (`scripts/prepare_images.py`) is included to resize and optimise images before copying them to the SD card.

### Requirements
```bash
# Install dependencies from uv.lock (first time or after pulling)
uv sync
```

### Usage
```bash
# Landscape (320×240) — default
uv run scripts/prepare_images.py -i ~/Photos -o /mnt/sdcard

# Portrait (240×320)
uv run scripts/prepare_images.py -i ~/Photos -o /mnt/sdcard --orientation portrait

# Both orientations at once
# Landscape images -> /mnt/sdcard/landscape/
# Portrait images  -> /mnt/sdcard/portrait/
uv run scripts/prepare_images.py -i ~/Photos -o /mnt/sdcard --orientation both

# Crop to fill instead of letterboxing
uv run scripts/prepare_images.py -i ~/Photos -o /mnt/sdcard --orientation both --fill

# Override dimensions manually (e.g. CYD-35C landscape)
uv run scripts/prepare_images.py -i ~/Photos -o /mnt/sdcard --width 480 --height 320

# Pre-generate both JPEGs and raw RGB565 files for instant boot
uv run scripts/prepare_images.py -i ~/Photos -o /mnt/sdcard --raw
```

| Flag | Default | Description |
|---|---|---|
| `--input` / `-i` | *(required)* | Source directory of images |
| `--output` / `-o` | *(required)* | Destination directory (or SD card mount) |
| `--orientation` | `landscape` | `landscape`, `portrait`, or `both` |
| `--width` | 320 / 240 | Override target width (ignored when `--orientation both`) |
| `--height` | 240 / 320 | Override target height (ignored when `--orientation both`) |
| `--fill` | off | Crop to fill instead of fitting with black bars |
| `--raw` | off | Pre-generate raw RGB565 files into `/cache/` alongside optimized JPEGs in `--output` |

> [!TIP]
> - **File Structure**: By default, optimized `.jpg` files are placed directly in `--output` (the SD card root). When `--raw` is enabled, the script creates a `cache/` subfolder in `--output` containing matching pre-rendered `<filename>_<width>x<height>.raw` files.
> - **Orientation Subdirectories**: When using `--orientation both`, images are written into `landscape/` and `portrait/` subdirectories. If `--raw` is also passed, a `cache/` folder is placed inside each orientation subdirectory.

## :computer: Development

### Running Tests
Unit tests can be run locally on your host machine without hardware:
```bash
pio test -e native
```

## :wrench: Troubleshooting

If you encounter any issues with your screen or the software, please review the solutions below or [create an issue](https://github.com/nicholaswilde/cyd-photo-frame/issues) on GitHub if you are still stuck.

### Inverted Colors
If the colors on your display appear inverted, this is a common issue with some batches of the CYD TFT screens. You can easily resolve it by:
- Flashing the pre-compiled `_inv` releases (e.g. `cyd-photo-frame-v0.1.4-cyd_28r_inv.zip` or `cyd-photo-frame-v0.1.4-cyd_35c_inv.zip`).
- Or, if building from source, using the `cyd_28r_inv` or `cyd_35c_inv` PlatformIO environments. These environments automatically enable the `TFT_INVERSION_ON=1` build flag.

### RGB / BGR Swap
If your screen has red and blue colors swapped, it means the display expects a BGR color order instead of RGB. A dedicated release is not currently provided for this variation, but you can fix it by building from source: simply append `-D TFT_RGB_ORDER=TFT_BGR` to your environment's `build_flags` in `platformio.ini`.

### Touch Calibration (Resistive Screens)
The 2.8" version (`cyd_28r`) uses a resistive touch layer which can sometimes be misaligned or mapped to inverted coordinates depending on the batch. If touches are registering in the wrong place, you may need to run a touch calibration sketch to determine the correct offsets for your specific screen.

### Backlight Pin and Polarity Differences
If your screen is completely black but the board seems to be running, it might be a backlight pin mapping issue. While most boards use GPIO 21 or 27 for the backlight (`TFT_BL`), some obscure batches might map it differently. Furthermore, some boards require `TFT_BACKLIGHT_ON=LOW` instead of `HIGH`. 

### Different Driver Chips
Most 2.8" screens use the `ILI9341` driver. However, occasionally manufacturers substitute it with an `ST7789V` driver without changing the board's appearance. This will result in a completely blank or noisy screen until you change the driver flag in your `platformio.ini` from `-D ILI9341_DRIVER=1` to `-D ST7789_DRIVER=1`.

### SD Card Formatting
The CYD board can be picky about SD cards. If you encounter a "Failed to initialize SD card" error:
- Ensure the microSD card is formatted as **FAT32** (not exFAT).
- Try to use an SD card that is 32GB or smaller.
- Ensure it is inserted fully before booting the device.

### Power Delivery Glitches
Intermittent touch issues, screen flickering, or spontaneous reboots are frequently caused by inadequate power. Ensure you are using a high-quality USB power supply and a good USB cable that can supply the burst current the ESP32 and backlight need simultaneously.

### Audio Output / DAC Pins
If you ever intend to use the speaker connector on the back, be aware that the DAC pin mapping changes between board revisions. Sometimes it's mapped to GPIO 25, and sometimes GPIO 26.

### "No RAM for frame buffer; falling back to SD seek-writes"
During the on-device JPEG optimization and caching phase, the firmware attempts to allocate a full-screen frame buffer in RAM (approx. 150–300 KB depending on screen resolution) using PSRAM or free heap memory to decode images entirely in RAM and write them to the SD card in a single sequential block.

If the board lacks external PSRAM or available contiguous heap memory is insufficient (such as when network and UI services are loaded), the console displays:
```text
[System] WARNING: No RAM for frame buffer; falling back to SD seek-writes.
```
This is **safe and expected behavior**. The device automatically falls back to streaming decoded rows directly to the SD card row-by-row (`SD seek-writes`). Initial conversion of un-cached photos will take slightly longer, but the resulting `.raw` images in `/cache/` will still load and render at full speed (<60ms) on all subsequent slideshow passes. You can also skip on-device optimization entirely by toggling **Bypass Optimization** or pre-generating `.raw` files using `scripts/prepare_images.py --raw`.

## :balance_scale: License

[Apache License 2.0](LICENSE)

## :writing_hand: Author

This project was started in 2026 by [Nicholas Wilde](https://github.com/nicholaswilde/).
