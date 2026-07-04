import bpy
import random
import math

# =========================
# CONFIG
# =========================

MIN_SCALE = 0.8
MAX_SCALE = 1.3

# =========================
# RANDOMIZE
# =========================

selected_objects = bpy.context.selected_objects

for obj in selected_objects:

    # Random uniform scale
    scale = random.uniform(MIN_SCALE, MAX_SCALE)

    # Set absolute scale (does NOT use current scale)
    obj.scale = (scale, scale, scale)

    # Random rotation around Z axis
    obj.rotation_euler.z = random.uniform(0, math.tau)

print(f"Randomized {len(selected_objects)} objects.")
