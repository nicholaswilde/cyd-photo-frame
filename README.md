# :framed_picture: CYD Photo Frame :pager:
[![task](https://img.shields.io/badge/Task-Enabled-brightgreen?style=for-the-badge&logo=task&logoColor=white)](https://taskfile.dev/#/)


A digital photo frame for the ESP32 Cheap Yellow Device (CYD) 

> [!WARNING]
> This project is currently in a `v0.X.X` development stage. Features and configurations are subject to change, and breaking changes may be introduced at any time.

## :hammer_and_wrench: Hardware Requirements

- **ESP32 Cheap Yellow Device (CYD)**:
  - **CYD 2.8" (Resistive)**: ESP32-2432S028R — 2.8″ 320×240 ILI9341 LCD with XPT2046 resistive touch.
  - **CYD 3.5" (Capacitive)**: ESP32-3248S035C — 3.5″ 480×320 ST7796 LCD with GT911/CST820 capacitive touch.
- **Storage**: MicroSD card slot (compatible with standard FAT32 formatted cards).
- Micro-USB / USB-C cable for power and programming.

## :star: Features

- **High-Performance Caching:** Converts JPEGs to raw RGB565 images on boot, enabling under **60ms** rendering times.
- **Catppuccin Theme Flavors:** Fully dynamic Settings panel and slideshow background borders in Mocha, Macchiato, Frappé, or Latte.
- **Auto-Brightness (LDR Sensor):** Automatically adjusts LCD backlight brightness depending on ambient room light levels.
- **Filename Banner Overlay:** Displays a clean, toggleable Catppuccin Mantle banner containing the current image name at the bottom of the screen.
- **Touch Navigation Zones:** Easily navigate images and access settings by tapping designated screen areas.

## :world_map: Touch Navigation Zones

The screen is divided into several touch zones to control behavior without visible UI clutter during the slideshow:
- **Left 20% of Screen:** Slide to the **previous** photo.
- **Right 20% of Screen:** Slide to the **next** photo.
- **Top 20% (Center):** **Toggle** the bottom filename banner display.
- **Bottom 20% (Center):** **Open** the LVGL Settings Menu.

## :floppy_disk: SD Card Configuration

> [!WARNING]
> **Auto-Formatting Warning:**
> On boot, if the SD card fails to mount (e.g., due to partition corruption or being raw/unformatted), the firmware is configured to automatically format the card to **FAT32**. 
> If you insert a corrupted card or experience connection issues, the card's contents **will be wiped**. Always keep backup copies of your images elsewhere.

1. Format your MicroSD card to **FAT32**.
2. Put your `.jpg` images directly into the root directory of the SD card.
3. Plug the card into the CYD SD slot. On boot, the ESP32 will auto-detect any new JPEGs, scale them to fit the screen keeping their aspect ratios, and cache them inside the `/cache/` directory.

> [!NOTE]
> **Display Orientation & Caching:**
> The device now supports four orientations (Landscape, Portrait, Landscape Rev, Portrait Rev).
> Cached images are stored per resolution (e.g., `_320x240.raw` for landscape modes and `_240x320.raw` for portrait modes), so switching between orientations no longer requires clearing the cache. Only a theme change clears the cache. 


## :usb: Serial Commands

If the CYD is plugged into your computer via USB:
1. Open the PlatformIO Serial Device Monitor (at `115200` baud).
2. Press **`Ctrl + T`** then **`Ctrl + E`** to enable local line editing/input mode.
3. Type **`clear`** or **`clear_cache`** and press **`Enter`** to clear the image cache.
4. Type **`screenshot`** and press **`Enter`** to capture the LVGL Settings screen (saved as BMP on the SD card).
5. Type **`screenshot_tft`** and press **`Enter`** to capture the current raw TFT screen (e.g., Optimization) as BMP on the SD card.
6. The ESP32 will format/empty the `/cache` directory (for clear) or write the BMP file, then automatically reboot if the cache was cleared.

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
```

| Flag | Default | Description |
|---|---|---|
| `--input` / `-i` | *(required)* | Source directory of images |
| `--output` / `-o` | *(required)* | Destination directory (or SD card mount) |
| `--orientation` | `landscape` | `landscape`, `portrait`, or `both` |
| `--width` | 320 / 240 | Override target width (ignored when `--orientation both`) |
| `--height` | 240 / 320 | Override target height (ignored when `--orientation both`) |
| `--fill` | off | Crop to fill instead of fitting with black bars |

> [!TIP]
> When using `--orientation both`, images are written into `landscape/` and `portrait/` subdirectories so you can copy only the set that matches your device's orientation setting.

## :computer: Development


### Compiling & Flashing
Select the environment matching your hardware:

```bash
# For CYD 2.8" (Resistive Touch, ILI9341)
pio run -e cyd_28r -t upload

# For CYD 3.5" (Capacitive Touch, ST7796)
pio run -e cyd_35c -t upload

# Start the Serial Monitor
pio device monitor
```

### Running Tests
Unit tests can be run locally on your host machine without hardware:
```bash
pio test -e native
```

## :balance_scale: License

[Apache License 2.0](LICENSE)

## :writing_hand: Author

This project was started in 2026 by [Nicholas Wilde](https://github.com/nicholaswilde/).
