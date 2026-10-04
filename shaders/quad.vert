#version 450
/* Porpoise: one textured rectangle, drawn as a 4-vertex triangle strip.
 * rect: x0, y0, x1, y1 in normalized device coordinates.
 * uv:   u0, v0, u1, v1 in texture coordinates. */
layout(push_constant) uniform Push {
    vec4 rect;
    vec4 uv;
    vec4 color;
} p;
layout(location = 0) out vec2 v_uv;
void main()
{
    vec2 corner = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
    gl_Position = vec4(mix(p.rect.xy, p.rect.zw, corner), 0.0, 1.0);
    v_uv = mix(p.uv.xy, p.uv.zw, corner);
}
