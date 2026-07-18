# Product Guidelines

## User Experience (UX) & Design Principles
- **Fast Startup**: Initial display setup must complete within 500ms of boot.
- **Minimalist Aesthetic**: The primary screen interface must only show the decompressed and centered photo. No diagnostic stats, text overlays, or status bars should cover the image.
- **Consistent Color Theme**: All fallback and error screens must use a unified color scheme. We adopt the **Catppuccin Mocha** palette:
  - Background (Base): `0x18E5` (dark charcoal/slate)
  - Error Text (Red): `0xF475` (rose/red)
  - Secondary Text (Text): `0xCE79` (off-white/light grey)
- **High-Quality Scaling**: TJpg_Decoder scale parameters must be used dynamically (1, 2, 4, 8) to fit images to the active display dimensions without distorting the aspect ratio.

## Development & Hardware Constraints
- **Low Memory Overhead**: Keep global/heap allocations minimal to avoid crashing on ESP32 (320KB RAM limit). Decompression should happen block-by-block (MCU blocks) directly to the screen.
- **I/O Safety**: Always verify hardware interfaces (SPI for TFT and SD card) before usage.
- **Serial Debugging**: Output structured logs via Serial at `115200` baud, prefixing categories like `[System]`, `[SD]`, `[Decoder]`, or `[Error]`.
