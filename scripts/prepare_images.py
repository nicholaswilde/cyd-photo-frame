#!/usr/bin/env python3
import os
import sys
import argparse

try:
    from PIL import Image, ImageOps
except ImportError:
    print("Error: The 'Pillow' library is required to run this script.")
    print("Install it using: pip install Pillow")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Resolution presets keyed by orientation
# (width, height) for landscape and portrait modes
# ---------------------------------------------------------------------------
LANDSCAPE_SIZE = (320, 240)
PORTRAIT_SIZE  = (240, 320)


def process_image(src_path, dest_path, width, height, crop_to_fill, generate_raw=False, cache_dir=None):
    try:
        with Image.open(src_path) as img:
            # Handle orientation from EXIF
            img = ImageOps.exif_transpose(img)

            # Convert to RGB mode (discard alpha channel/transparency from PNGs/WebPs)
            if img.mode != 'RGB':
                img = img.convert('RGB')

            # Resize
            if crop_to_fill:
                # Crop to fill (maintain aspect ratio, crop excess)
                img = ImageOps.fit(img, (width, height), Image.Resampling.LANCZOS)
            else:
                # Fit within boundaries (pad with black bars)
                img.thumbnail((width, height), Image.Resampling.LANCZOS)
                background = Image.new('RGB', (width, height), (0, 0, 0))
                offset = ((width - img.width) // 2, (height - img.height) // 2)
                background.paste(img, offset)
                img = background

            # Save as standard (baseline) JPEG
            img.save(dest_path, "JPEG", quality=85, progressive=False)

            # Save raw RGB565 file if requested
            if generate_raw and cache_dir:
                os.makedirs(cache_dir, exist_ok=True)
                base_name = os.path.splitext(os.path.basename(dest_path))[0]
                raw_filename = f"{base_name}_{width}x{height}.raw"
                raw_path = os.path.join(cache_dir, raw_filename)
                
                # Convert pixel values to raw RGB565 bytes (big-endian)
                pixels = list(img.getdata())
                raw_bytes = bytearray(len(pixels) * 2)
                idx = 0
                for r, g, b in pixels:
                    val = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                    raw_bytes[idx] = (val >> 8) & 0xFF
                    raw_bytes[idx + 1] = val & 0xFF
                    idx += 2
                    
                with open(raw_path, "wb") as f:
                    f.write(raw_bytes)

            return True
    except Exception as e:
        print(f"Failed to process {src_path}: {e}")
        return False


def run_batch(files, input_dir, output_dir, width, height, crop_to_fill, label, generate_raw=False):
    """Process a list of image files into output_dir at the given resolution."""
    os.makedirs(output_dir, exist_ok=True)
    
    cache_dir = os.path.join(output_dir, "cache") if generate_raw else None

    print(f"\n[{label}] {width}x{height} | Mode: {'Fill' if crop_to_fill else 'Fit'} | Output: {output_dir}")
    if generate_raw:
        print(f"[{label}] Raw Cache Output: {cache_dir}")
    print(f"Processing {len(files)} image(s)...")

    success_count = 0
    total_saved_bytes = 0
    total_orig_bytes = 0

    for filename in files:
        src_path = os.path.join(input_dir, filename)
        dest_filename = os.path.splitext(filename)[0] + ".jpg"
        dest_path = os.path.join(output_dir, dest_filename)

        orig_size = os.path.getsize(src_path)
        total_orig_bytes += orig_size

        if process_image(src_path, dest_path, width, height, crop_to_fill, generate_raw, cache_dir):
            new_size = os.path.getsize(dest_path)
            total_saved_bytes += (orig_size - new_size)
            success_count += 1
            raw_info = " + RAW" if generate_raw else ""
            print(f"  [OK] {filename} -> {dest_filename} ({new_size / 1024:.1f} KB){raw_info}")

    print(f"\n--- {label} Summary ---")
    print(f"Successfully processed: {success_count}/{len(files)} images.")
    if success_count > 0 and total_orig_bytes > 0:
        saved_mb = total_saved_bytes / (1024 * 1024)
        orig_mb  = total_orig_bytes  / (1024 * 1024)
        new_mb   = (total_orig_bytes - total_saved_bytes) / (1024 * 1024)
        print(f"Original size:  {orig_mb:.2f} MB")
        print(f"Optimized size: {new_mb:.2f} MB")
        print(f"Saved:          {saved_mb:.2f} MB ({total_saved_bytes / total_orig_bytes * 100:.1f}% reduction)")

    return success_count


def main():
    parser = argparse.ArgumentParser(
        description="Prepare images for the CYD Photo Frame.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Orientation modes:
  landscape  – Optimise for landscape display (320x240 by default).
               Images are written directly to --output.
  portrait   – Optimise for portrait display  (240x320 by default).
               Images are written directly to --output.
  both       – Produce both sets. Landscape images go to --output/landscape/
               and portrait images go to --output/portrait/.

When using --width / --height together with --orientation landscape or portrait,
the explicit dimensions override the defaults for that orientation.
        """
    )
    parser.add_argument("--input",  "-i", required=True,
                        help="Path to the directory containing source images.")
    parser.add_argument("--output", "-o", required=True,
                        help="Path to the output directory (e.g. SD card mount point).")
    parser.add_argument("--orientation", choices=["landscape", "portrait", "both"],
                        default="landscape",
                        help="Target orientation: landscape, portrait, or both (default: landscape).")
    parser.add_argument("--width",  type=int, default=None,
                        help="Override target width  (default: 320 for landscape, 240 for portrait).")
    parser.add_argument("--height", type=int, default=None,
                        help="Override target height (default: 240 for landscape, 320 for portrait).")
    parser.add_argument("--fill", action="store_true",
                        help="Crop images to fill the screen completely (default: fit with black bars).")
    parser.add_argument("--raw", action="store_true",
                        help="Generate raw RGB565 files directly in the '/cache' directory for instant boot slideshow.")

    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: Input directory '{args.input}' does not exist.")
        sys.exit(1)

    supported_extensions = ('.jpg', '.jpeg', '.png', '.webp', '.bmp', '.gif', '.heic', '.tiff')
    files = [f for f in os.listdir(args.input)
             if os.path.splitext(f)[1].lower() in supported_extensions]
    if not files:
        print(f"No supported images found in '{args.input}'.")
        sys.exit(0)

    # Build list of (label, width, height, output_dir) jobs to run
    jobs = []

    if args.orientation == "both":
        if args.width or args.height:
            print("Warning: --width / --height are ignored when --orientation=both.")
        jobs.append(("Landscape", LANDSCAPE_SIZE[0], LANDSCAPE_SIZE[1],
                      os.path.join(args.output, "landscape")))
        jobs.append(("Portrait",  PORTRAIT_SIZE[0],  PORTRAIT_SIZE[1],
                      os.path.join(args.output, "portrait")))
    else:
        default_w, default_h = (LANDSCAPE_SIZE if args.orientation == "landscape"
                                 else PORTRAIT_SIZE)
        w = args.width  if args.width  is not None else default_w
        h = args.height if args.height is not None else default_h
        jobs.append((args.orientation.capitalize(), w, h, args.output))

    total_success = 0
    for label, w, h, out_dir in jobs:
        total_success += run_batch(files, args.input, out_dir, w, h, args.fill, label, generate_raw=args.raw)

    if len(jobs) > 1:
        print(f"\n{'='*40}")
        print(f"All done. Total images processed: {total_success}/{len(files) * len(jobs)}")


if __name__ == "__main__":
    main()
