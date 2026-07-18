# Initial Concept
A digital photo frame for the ESP32 Cheap Yellow Device (CYD).

# CYD Digital Photo Frame

## Overview
A lightweight, robust digital photo frame firmware for ESP32 Cheap Yellow Device (CYD) development boards. The system automatically reads, scales, centers, and cycles through JPEG images stored on a MicroSD card.

## Core Objectives
- **Simplicity**: No complex initial setups or external APIs needed. Insert a MicroSD card containing JPEGs, power up the device, and start the slideshow.
- **Hardware Agnostic (CYD)**: Support both CYD-28R (2.8", ILI9341, 320x240) and CYD-35C (3.5", ST7796, 480x320) targets from a unified codebase.
- **Reliability**: Provide clear visual errors if the SD card is missing or files cannot be parsed, resuming automatically where possible.

## Features
- **SD Card Slideshow**: Automatic directory parsing of the MicroSD card root (`/`) to find `.jpg` / `.jpeg` files.
- **Dynamic Image Scaling**: Compute scaling factors (1/1, 1/2, 1/4, 1/8) based on image aspect ratio relative to the active display dimensions.
- **Centering & Padding**: Calculate X and Y offsets dynamically to center the scaled image on the display, filling the background with a clean color (Catppuccin Mocha base).
- **Infinite Slideshow Loop**: Seamlessly cycle through files, automatically rewinding to the beginning when reaching the end of the directory.
- **Touch Controls**: Tap Left zone (0%-30% width) for previous image, Right zone (70%-100% width) for next image, and Center zone (30%-70% width) to pause/resume slideshow.
- **Error Handling**: Graceful error UI indicating SD card connection states or corrupt/unsupported formats.
