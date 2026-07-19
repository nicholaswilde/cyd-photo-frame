#!/usr/bin/env python3
"""Convert BMP screenshots captured from the CYD device into PNG files.

Place .bmp files from the SD card's /screenshots/ directory into the local
screenshots/ folder, then run this script to convert and delete the originals.
"""
import os
import glob
from PIL import Image


def main():
    screenshots_dir = "screenshots"
    if not os.path.isdir(screenshots_dir):
        print(f"Directory '{screenshots_dir}' does not exist.")
        print("Create it and copy the .bmp files from the SD card first.")
        return

    bmp_files = glob.glob(os.path.join(screenshots_dir, "*.bmp"))
    if not bmp_files:
        print(f"No .bmp files found in '{screenshots_dir}/'.")
        return

    print(f"Found {len(bmp_files)} file(s) to convert...")

    for bmp_path in bmp_files:
        png_path = os.path.splitext(bmp_path)[0] + ".png"
        print(f"  Converting {bmp_path} -> {png_path} ...", end=" ")
        try:
            with Image.open(bmp_path) as img:
                img.save(png_path, "PNG")
            os.remove(bmp_path)
            print("OK")
        except Exception as e:
            print(f"FAILED ({e})")

    print("Done.")


if __name__ == "__main__":
    main()
