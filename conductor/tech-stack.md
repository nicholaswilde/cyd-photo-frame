# Technology Stack

## Core Development Environment
- **Programming Language**: C++ (C++11/C++14 standards)
- **Framework**: Arduino Core for ESP32
- **Build System & IDE Support**: PlatformIO CLI / IDE

## Hardware Targets
- **SoC**: Espressif ESP32-D0WDQ6 (dual-core XTensa LX6 @ 240MHz)
- **Boards**:
  - **CYD-28R** (ESP32-2432S028R) — 2.8" LCD, ILI9341 Driver, SPI interface, SPI Resistive Touch (XPT2046)
  - **CYD-35C** (ESP32-3248S035C) — 3.5" LCD, ST7796 Driver, SPI interface, I2C Capacitive Touch (GT911/CST820)

## Library Dependencies
- **bodmer/TFT_eSPI** (v2.5.43): High-performance graphics library for TFT displays.
- **bodmer/TJpg_Decoder** (v1.1.0): Lightweight JPEG decoder using Tiny JPEG Decompressor library (tjpgd).
- **SD**: Default ESP32 Arduino SD card library using SPI.
- **SPI**: Native SPI library for bus communication.
- **XPT2046_Touchscreen**: SPI touchscreen controller library (used conditionally for CYD-28R).
- **bblanchon/ArduinoJson** (v6.21.3): JSON parser and generator.
- **me-no-dev/AsyncTCP** (v1.1.1) & **marvinroger/AsyncMqttClient** (v0.9.0): Async communication wrappers (retained for future features).

## Python Tooling

- **Runtime Manager**: [`uv`](https://docs.astral.sh/uv/) — all Python scripts **must** be run via `uv run`. Never invoke `python`, `python3`, or `pip` directly.
- **Package Management**: `uv add` / `uv remove` — dependency changes are declared in `pyproject.toml` and pinned in `uv.lock`.
- **Project Config**: `pyproject.toml` (PEP 517/518/735 compliant) at the repository root.
- **Lock File**: `uv.lock` — must be committed and kept up to date.
- **Python Version**: Pinned in `.python-version` (currently `3.13`).

### Rules
1. Scripts are invoked as `uv run scripts/<name>.py [args]` — uv auto-creates / reuses the virtualenv.
2. Installing dependencies: `uv add <package>` (updates `pyproject.toml` + `uv.lock` atomically).
3. Never commit a bare `requirements.txt` as the source of truth; generate one only if an external tool requires it (`uv export --format requirements-txt`).
4. Taskfile tasks that call Python scripts must use `uv run`, not `python3`.

