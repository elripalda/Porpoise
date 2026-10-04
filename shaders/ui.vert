#version 450
/* Porpoise UI: every element is a quad of six vertices whose positions arrive
 * already projected (clip space with w), so covers turned in perspective are
 * textured perspective-correct. */
layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec2 a_local;
layout(location = 3) in vec4 a_color;
layout(location = 4) in vec4 a_p0;
layout(location = 5) in vec4 a_p1;
layout(location = 6) in vec4 a_p2;
layout(location = 7) in vec4 a_bcolor;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec2 v_local;
layout(location = 2) out vec4 v_color;
layout(location = 3) flat out vec4 v_p0;
layout(location = 4) flat out vec4 v_p1;
layout(location = 5) flat out vec4 v_p2;
layout(location = 6) flat out vec4 v_bcolor;

void main()
{
    gl_Position = a_pos;
    v_uv = a_uv;
    v_local = a_local;
    v_color = a_color;
    v_p0 = a_p0;
    v_p1 = a_p1;
    v_p2 = a_p2;
    v_bcolor = a_bcolor;
}
