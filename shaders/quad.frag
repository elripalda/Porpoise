#version 450
layout(push_constant) uniform Push {
    vec4 rect;
    vec4 uv;
    vec4 color;
} p;
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 frag;
void main()
{
    frag = texture(tex, v_uv) * p.color;
}
