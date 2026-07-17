#pragma once

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#define SD_MAX_BONES_WGPU 128
#define SD_MAX_COLORS_WGPU 8
#define SD_MAX_CASCADES 4

#define SHADER_DEFS_WGSL \
    "const MAX_BONES: u32 = " STRINGIFY(SD_MAX_BONES_WGPU) ";\n" \
    "const MAX_COLORS: u32 = " STRINGIFY(SD_MAX_COLORS_WGPU) ";\n" \
    "const MAX_CASCADES: u32 = " STRINGIFY(SD_MAX_CASCADES) ";\n" \
    "\n"
//
//#undef STRINGIFY_HELPER
//#undef STRINGIFY
