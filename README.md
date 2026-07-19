# :framed_picture: CYD Photo Frame :pager:
[![task](https://img.shields.io/badge/Task-Enabled-brightgreen?style=for-the-badge&logo=task&logoColor=white)](https://taskfile.dev/#/)


A digital photo frame for the ESP32 Cheap Yellow Device (CYD) 

> [!WARNING]
> This project is currently in a `v0.X.X` development stage. Features and configurations are subject to change, and breaking changes may be introduced at any time.

## :hammer_and_wrench: Hardware Requirements

- **ESP32 Cheap Yellow Device (CYD)**: ESP32-2432S028R — 2.8″ 320×240 ILI9341 LCD with XPT2046 resistive touch.
- **Storage**: MicroSD card slot (compatible with standard FAT32 formatted cards).
- Micro-USB cable for power and programming.

## :star: Features

- **High-Performance Caching:** Converts JPEGs to raw RGB565 images on boot, enabling under **60ms** rendering times.
- **Catppuccin Theme Flavors:** Fully dynamic Settings panel and slideshow background borders in Mocha, Macchiato, Frappé, or Latte.
- **Serial Commands:** Manage and clear cache directories remotely over USB.

## :usb: Serial Commands (Clearing Cache)

If the CYD is plugged into your computer via USB:
1. Open the PlatformIO Serial Device Monitor (at `115200` baud).
2. Press **`Ctrl + T`** then **`Ctrl + E`** to enable local line editing/input mode.
3. Type **`clear`** or **`clear_cache`** and press **`Enter`**.
4. The ESP32 will format/empty the `/cache/` directory on the SD card and automatically reboot itself to regenerate the caching borders.

## :computer: Development

## :balance_scale: License

[Apache License 2.0](LICENSE)

## :writing_hand: Author

This project was started in 2026 by [Nicholas Wilde](https://github.com/nicholaswilde/).
