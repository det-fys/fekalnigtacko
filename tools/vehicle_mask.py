#!/usr/bin/env python3

"""
Vehicle texture mask encoder
----------------------------

Loads:
    <name>_src.png
    <name>_mask.png

Produces:
    <name>.png

The RGB channels come from the source texture.
The alpha channel encodes color slots based on the mask texture.

Mask colors are mapped to alpha intervals:
    0..7  = configurable slots
    8     = "no tint"

The alpha range is divided into 9 equal buckets.
Each slot uses the CENTER of its bucket for stability in shaders,
because apparently graphics pipelines enjoy sabotaging precision
for sport.

Requirements:
    pip install pillow
"""

from PIL import Image
import sys
from pathlib import Path


# ============================================================================
# CONFIGURATION
# ============================================================================

# Define your 8 slot colors here.
# These should match the colors used in your *_mask.png images.
#
# Format:
#     (R, G, B)
#
# Example palette:
MASK_SLOT_COLORS = [
    (255, 255, 255), # primary
    (128, 128, 128), # secondary

    (0, 38, 255), # headlights
    (255, 0, 0), # rear lights
    (127, 0, 0), # braking lights
    (255, 106, 0), # orange lights
    (178, 0, 255), # reverse light

    (0, 255, 255), # other
]

# Black in the mask image means:
# "no tint"
NO_TINT_COLOR = (0, 0, 0)

# Color matching tolerance.
# Increase if your editor slightly alters colors.
COLOR_TOLERANCE = 0


# ============================================================================
# INTERNALS
# ============================================================================

NUM_INTERVALS = 9
INTERVAL_SIZE = 256 / NUM_INTERVALS


def interval_center(index: int) -> int:
    """
    Returns the alpha value at the center of the interval.
    """

    center = (index + 0.5) * INTERVAL_SIZE
    return max(0, min(255, round(center)))


def color_matches(c1, c2, tolerance=0):
    return (
        abs(c1[0] - c2[0]) <= tolerance and
        abs(c1[1] - c2[1]) <= tolerance and
        abs(c1[2] - c2[2]) <= tolerance
    )


def find_slot(color):
    """
    Returns:
        0..7 for tint slots
        8    for no tint
        None if unknown
    """

    if color_matches(color, NO_TINT_COLOR, COLOR_TOLERANCE):
        return 8
    
    for i, slot_color in enumerate(MASK_SLOT_COLORS):
        if color_matches(color, slot_color, COLOR_TOLERANCE):
            return i
        
    return None


def main():
    if len(sys.argv) != 2:
        print("Usage:")
        print("    python vehicle_mask_encoder.py <name>")
        sys.exit(1)

    name = sys.argv[1]

    src_path = Path(f"{name}_src.png")
    mask_path = Path(f"{name}_mask.png")
    out_path = Path(f"{name}.png")

    if not src_path.exists():
        print(f"Missing source image: {src_path}")
        sys.exit(1)

    if not mask_path.exists():
        print(f"Missing mask image: {mask_path}")
        sys.exit(1)

    src_img = Image.open(src_path).convert("RGBA")
    mask_img = Image.open(mask_path).convert("RGB")

    if src_img.size != mask_img.size:
        print("Source and mask image sizes do not match.")
        sys.exit(1)

    width, height = src_img.size

    out_img = Image.new("RGBA", (width, height))

    src_pixels = src_img.load()
    mask_pixels = mask_img.load()
    out_pixels = out_img.load()

    unknown_colors = set()

    for y in range(height):
        for x in range(width):

            sr, sg, sb, _ = src_pixels[x, y]
            mask_color = mask_pixels[x, y]

            slot = find_slot(mask_color)

            if slot is None:
                unknown_colors.add(mask_color)

                # Fallback to no tint
                slot = 8

            alpha = interval_center(slot)

            out_pixels[x, y] = (sr, sg, sb, alpha)

    out_img.save(out_path)

    print(f"Saved: {out_path}")

    print("\nAlpha encoding:")
    for i in range(NUM_INTERVALS):
        alpha = interval_center(i)

        if i == 8:
            label = "NO TINT"
        else:
            label = f"SLOT {i}"

        print(f"  {label:<10} -> alpha {alpha}")

    if unknown_colors:
        print("\nWarning: unknown mask colors encountered:")
        for c in sorted(unknown_colors):
            print(f"  {c}")

        print("\nThose pixels were encoded as NO TINT.")
        print("Humanity invents entire rendering pipelines and still")
        print("cannot agree on whether #FE0000 is 'red enough'.")


if __name__ == "__main__":
    main()
