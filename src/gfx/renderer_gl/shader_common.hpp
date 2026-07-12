#pragma once

#include "shader_defs.hpp"
#include "gl.hpp"

#ifndef PG_GLES
#define GLSL_VERSION \
    "#version 330 core\n" \
    "\n"
#else 
#define GLSL_VERSION \
    "#version 300 es\n" \
    "precision highp float;\n" \
    "\n"
#endif

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#define SHADER_DEFS \
    "#define MAX_LIGHTS " STRINGIFY(SD_MAX_LIGHTS) "\n" \
    "#define MAX_BONES " STRINGIFY(SD_MAX_BONES) "\n" \
    "#define MAX_COLORS " STRINGIFY(SD_MAX_COLORS) "\n" \
    "\n"

#define SHADER_HEADER \
    GLSL_VERSION \
    SHADER_DEFS
