#include "shader_sources.hpp"
#include "shader_common.hpp"

// Zdrojove kody shaderu
static const char* const s_srcs[] = {

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

// SS_HUD_VERT
SHADER_HEADER
R"GLSL(
layout (location = 0) in vec3 a_pos;
layout (location = 2) in vec4 a_color;
layout (location = 3) in vec2 a_uv;

uniform mat3 u_model;

out vec4 v_color;
out vec2 v_uv;

void main() {
    vec3 pos2d = u_model * vec3(a_pos.xy, 1.0);
    gl_Position = vec4(pos2d.xy, a_pos.z, 1.0);
    v_color = a_color;
    v_uv = a_uv;
}	
)GLSL",

// SS_HUD_FRAG
SHADER_HEADER
R"GLSL(

in vec4 v_color;
in vec2 v_uv;

uniform sampler2D u_tex;

layout (location = 0) out vec4 o_color;

void main() {
    o_color = texture(u_tex, v_uv);
    o_color *= v_color;
}	

)GLSL",


// SS_BEAM_VERT
SHADER_HEADER
R"GLSL(
layout (location = 0) in vec3 a_pos;

// instance
layout (location = 2) in vec4 a_color;
layout (location = 10) in vec3 a_p0;
layout (location = 11) in vec3 a_p1;
layout (location = 12) in float a_radius;

uniform mat4 u_view_proj;
uniform vec3 u_camera; 

out vec4 v_color;

void main() {
    vec3 p = mix(a_p0, a_p1, a_pos.y);

    vec3 seg_dir = a_p1 - a_p0;
    vec3 cam_dir = u_camera - p;
    vec3 cross_dir = normalize(cross(seg_dir, cam_dir));

    p += cross_dir * a_radius * (a_pos.x - 0.5) * 2.0;
    gl_Position = u_view_proj * vec4(p, 1.0);
    v_color = a_color;
}	
)GLSL",

// SS_BEAM_FRAG
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
