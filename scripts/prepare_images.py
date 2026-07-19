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

def process_image(src_path, dest_path, width, height, crop_to_fill):
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
            return True
    except Exception as e:
        print(f"Failed to process {src_path}: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description="Prepare images for the CYD Photo Frame.")
    parser.add_argument("--input", "-i", required=True, help="Path to the directory containing source images.")
    parser.add_argument("--output", "-o", required=True, help="Path to the output directory (e.g. SD card mount point).")
    parser.add_argument("--width", type=int, default=320, help="Target screen width (default: 320 for CYD-28R, use 480 for CYD-35C).")
    parser.add_argument("--height", type=int, default=240, help="Target screen height (default: 240 for CYD-28R, use 320 for CYD-35C).")
    parser.add_argument("--fill", action="store_true", help="Crop images to fill the screen completely (default: fit with black bars).")

    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: Input directory '{args.input}' does not exist.")
        sys.exit(1)

    os.makedirs(args.output, exist_ok=True)

    supported_extensions = ('.jpg', '.jpeg', '.png', '.webp', '.bmp', '.gif', '.heic', '.tiff')
    
    files = [f for f in os.listdir(args.input) if os.path.splitext(f)[1].lower() in supported_extensions]
    if not files:
        print(f"No supported images found in '{args.input}'.")
        sys.exit(0)

    print(f"Processing {len(files)} images to {args.width}x{args.height} (Mode: {'Fill' if args.fill else 'Fit'})...")
    
    success_count = 0
    total_saved_bytes = 0
    total_orig_bytes = 0

    for filename in files:
        src_path = os.path.join(args.input, filename)
        dest_filename = os.path.splitext(filename)[0] + ".jpg"
        dest_path = os.path.join(args.output, dest_filename)

        orig_size = os.path.getsize(src_path)
        total_orig_bytes += orig_size

        if process_image(src_path, dest_path, args.width, args.height, args.fill):
            new_size = os.path.getsize(dest_path)
            total_saved_bytes += (orig_size - new_size)
            success_count += 1
            print(f"  [OK] {filename} -> {dest_filename} ({new_size / 1024:.1f} KB)")

    print("\n--- Summary ---")
    print(f"Successfully processed: {success_count}/{len(files)} images.")
    if success_count > 0:
        saved_mb = total_saved_bytes / (1024 * 1024)
        orig_mb = total_orig_bytes / (1024 * 1024)
        new_mb = (total_orig_bytes - total_saved_bytes) / (1024 * 1024)
        print(f"Original size: {orig_mb:.2f} MB")
        print(f"Optimized size: {new_mb:.2f} MB")
        print(f"Saved: {saved_mb:.2f} MB ({total_saved_bytes / total_orig_bytes * 100:.1f}% reduction)")

if __name__ == "__main__":
    main()
