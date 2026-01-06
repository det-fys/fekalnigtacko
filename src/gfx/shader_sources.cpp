#include "shader_sources.hpp"
#include "shader_defs.hpp"
#include "client/gl.hpp"

#ifndef PG_GLES
#define GLSL_VERSION \
    "#version 330 core\n" \
    "\n"
#else 
#define GLSL_VERSION \
    "#version 300 es\n" \
    "precision mediump float;\n" \
    "\n"
#endif

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#define SHADER_DEFS \
    "#define MAX_LIGHTS " STRINGIFY(SD_MAX_LIGHTS) "\n" \
    "#define MAX_BONES " STRINGIFY(SD_MAX_BONES) "\n" \
    "\n"

#define SHADER_HEADER \
    GLSL_VERSION \
    SHADER_DEFS

#define MESH_MATRICES_GLSL R"GLSL(
    uniform mat4 u_view_proj;
    uniform mat4 u_model; // Transform matrix 
)GLSL"

#define LIGHT_MATRICES_GLSL R"GLSL(
    uniform vec3 u_ambient_light;
    uniform int u_num_lights;
    uniform vec3 u_light_positions[MAX_LIGHTS];
    uniform vec4 u_light_colors_rs[MAX_LIGHTS]; // rgb = color, a = radius
)GLSL"

#define COMPUTE_LIGHTS_GLSL R"GLSL(
// Example sun values (can later be uniforms)
vec3 u_sun_direction = normalize(vec3(0.3, 0.5, -0.8)); // direction from which sunlight comes
vec3 u_sun_color = vec3(1.0, 0.95, 0.7);              // warm sunlight color

vec3 ComputeLights(in vec3 sector_pos, in vec3 sector_normal)
{
    // Base ambient
    vec3 color = vec3(0.5, 0.5, 0.5); // u_ambient_light

    // Sunlight contribution
    float sun_dot = max(dot(sector_normal, -u_sun_direction), 0.0);
    color += u_sun_color * sun_dot;

    // Point lights
    for (int i = 0; i < u_num_lights; ++i) {
        vec3 light_pos = u_light_positions[i];
        vec3 light_color = u_light_colors_rs[i].rgb;
        float light_radius = u_light_colors_rs[i].a;
        
        vec3 to_light = light_pos - sector_pos;
        float dist2 = dot(to_light, to_light);
        if (dist2 < light_radius * light_radius) {
            float dist = sqrt(dist2);
            float attenuation = 1.0 - (dist / light_radius);
            float dot_term = max(dot(sector_normal, normalize(to_light)), 0.0);
            color += light_color * dot_term * attenuation;
        }
    }

    return color;
}
)GLSL"

// Zdrojove kody shaderu
static const char* const s_srcs[] = {

// SS_MESH_VERT
SHADER_HEADER
R"GLSL(
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_color;
layout (location = 3) in vec2 a_uv;

)GLSL"
MESH_MATRICES_GLSL
LIGHT_MATRICES_GLSL
COMPUTE_LIGHTS_GLSL
R"GLSL(

out vec2 v_uv;
out vec3 v_color;

void main() {
    vec4 world_pos = u_model * vec4(a_pos, 1.0);
    vec3 world_normal = normalize(mat3(u_model) * a_normal);
    gl_Position = u_view_proj * world_pos;

    v_uv = vec2(a_uv.x, 1.0 - a_uv.y);
    v_color = ComputeLights(world_pos.xyz, world_normal) * a_color;
}	
)GLSL",

// SS_MESH_FRAG
SHADER_HEADER
R"GLSL(
in vec2 v_uv;
in vec3 v_color;

uniform sampler2D u_tex;

layout (location = 0) out vec4 o_color;

void main() {
    o_color = vec4(texture(u_tex, v_uv));
    o_color.rgb *= v_color; // Apply vertex color
    //o_color = vec4(1.0, 0.0, 0.0, 1.0);
}	

)GLSL",

// SS_SKEL_MESH_VERT
SHADER_HEADER
R"GLSL(
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;
layout (location = 3) in vec2 a_uv;
layout (location = 5) in ivec4 a_bone_ids;
layout (location = 6) in vec4 a_bone_weights;

layout (std140) uniform Bones {
    mat4 u_bone_matrices[MAX_BONES];
};

)GLSL"
MESH_MATRICES_GLSL
LIGHT_MATRICES_GLSL
COMPUTE_LIGHTS_GLSL
R"GLSL(

out vec2 v_uv;

out vec3 v_color;

void main() {
    mat4 bone_transform = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        int bone_id = a_bone_ids[i];
        if (bone_id >= 0) {
            bone_transform += u_bone_matrices[bone_id] * a_bone_weights[i];
        }
    }

    vec4 world_pos = u_model * bone_transform * vec4(a_pos, 1.0);
    vec3 world_normal = normalize(mat3(u_model) * mat3(bone_transform) * a_normal);
    gl_Position = u_view_proj * world_pos;

    v_uv = vec2(a_uv.x, 1.0 - a_uv.y);

    v_color = ComputeLights(world_pos.xyz, world_normal);
}	
)GLSL",

// SS_SKEL_MESH_FRAG
SHADER_HEADER
R"GLSL(
in vec2 v_uv;

in vec3 v_color;

uniform sampler2D u_tex;

layout (location = 0) out vec4 o_color;

void main() {
    o_color = vec4(texture(u_tex, v_uv));
    o_color.rgb *= v_color; // Apply vertex color
}	

)GLSL",

// SS_SOLID_VERT
SHADER_HEADER
R"GLSL(
layout (location = 0) in vec3 a_pos;
layout (location = 2) in vec4 a_color;

uniform mat4 u_view_proj;
uniform vec4 u_color;

out vec4 v_color;

void main() {
    gl_Position = u_view_proj * vec4(a_pos, 1.0);
    v_color = a_color * u_color;
}	
)GLSL",

// SS_SOLID_FRAG
SHADER_HEADER
R"GLSL(
in vec4 v_color;

layout (location = 0) out vec4 o_color;

void main() {
    o_color = v_color;
}	

)GLSL",


};

// Vrati zdrojovy kod shaderu
const char* gfx::ShaderSources::Get(ShaderSource ss) {
	return s_srcs[ss];
}
