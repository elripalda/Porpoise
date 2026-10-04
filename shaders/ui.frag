#version 450
/* Porpoise UI fragment shader. One pipeline, one shader: p0.x picks what the
 * quad is.
 *
 *  p0 = (kind, corner radius px, border px, glow px)
 *  p1 = (quad w px, quad h px, shape w px, shape h px)   shape <= quad (glow margin)
 *  p2 = (bottom gradient multiplier, top sheen, subtype, rotation / extra)
 *  bcolor = border and glow colour
 */
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec2 v_local;
layout(location = 2) in vec4 v_color;
layout(location = 3) flat in vec4 v_p0;
layout(location = 4) flat in vec4 v_p1;
layout(location = 5) flat in vec4 v_p2;
layout(location = 6) flat in vec4 v_bcolor;

layout(set = 0, binding = 0) uniform sampler2D tex;
layout(push_constant) uniform Push {
    vec4 screen; /* width px, height px, time s, background dim 0..1 */
    vec4 extra;  /* reduced motion, -, -, - */
} pc;

layout(location = 0) out vec4 o;

const int K_IMAGE = 0;
const int K_PANEL = 1;
const int K_TEXT = 2;
const int K_BACKGROUND = 3;
const int K_GLYPH = 4;
const int K_COVER = 5;
const int K_BLOB = 6;
const int K_REFLECTION = 7;
const int K_GLASS = 8;

float sdRoundBox(vec2 p, vec2 b, float r)
{
    r = min(r, min(b.x, b.y));
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float sdSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

/* Equilateral triangle pointing up on screen (y grows downward). */
float sdTriangleUp(vec2 p, float r)
{
    p.y = -p.y;
    const float k = sqrt(3.0);
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0)
        p = vec2(p.x - k * p.y, -k * p.x - p.y) / 2.0;
    p.x -= clamp(p.x, -2.0 * r, 0.0);
    return -length(p) * sign(p.y);
}

vec2 rotate(vec2 p, float a)
{
    float c = cos(a), s = sin(a);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float coverage(float d)
{
    float aa = max(fwidth(d), 1e-4) * 0.75;
    return 1.0 - smoothstep(-aa, aa, d);
}

/* The grid room: a floor in perspective, a faint back wall, and slow light. */
vec4 background()
{
    vec2 res = pc.screen.xy;
    vec2 uv = v_local;
    float t = pc.screen.z * (1.0 - pc.extra.x);
    float aspect = res.x / res.y;
    float horizon = 0.53;

    vec3 col = mix(vec3(0.010, 0.016, 0.040), vec3(0.016, 0.028, 0.075), smoothstep(0.0, horizon, uv.y));
    float lines = 0.0;

    if (uv.y > horizon)
    {
        float depth = 0.30 / (uv.y - horizon);
        vec2 g = vec2((uv.x - 0.5) * aspect * depth * 7.0, depth * 7.0 + t * 0.30);
        vec2 f = abs(fract(g + 0.5) - 0.5) / fwidth(g);
        float l = 1.0 - min(min(f.x, f.y) * 1.1, 1.0);
        lines = l * 0.62 * exp(-depth * 0.10) * smoothstep(horizon, horizon + 0.05, uv.y);
        col += vec3(0.010, 0.020, 0.060) * smoothstep(horizon, 1.0, uv.y);
    }
    else
    {
        vec2 g = vec2((uv.x - 0.5) * aspect * 16.0, (horizon - uv.y) * 16.0);
        vec2 f = abs(fract(g + 0.5) - 0.5) / fwidth(g);
        float l = 1.0 - min(min(f.x, f.y) * 1.1, 1.0);
        lines = l * 0.30 * smoothstep(0.05, horizon, uv.y);
    }

    /* Broad soft lights that drift slowly. */
    vec2 p = vec2(uv.x * aspect, uv.y);
    vec2 cyan_at = vec2((0.16 + 0.05 * sin(t * 0.11)) * aspect, 0.86 + 0.03 * sin(t * 0.07));
    vec2 lav_at = vec2((0.86 + 0.04 * sin(t * 0.09 + 1.7)) * aspect, 0.74 + 0.04 * cos(t * 0.08));
    vec2 mid_at = vec2((0.5 + 0.18 * sin(t * 0.045)) * aspect, 0.95);
    float cyan = exp(-dot(p - cyan_at, p - cyan_at) / 0.10);
    float lav = exp(-dot(p - lav_at, p - lav_at) / 0.09);
    float mid = exp(-dot(p - mid_at, p - mid_at) / 0.05);
    /* Two soft beams from the top, swaying a little. */
    float beam_l = exp(-pow((uv.x - (0.22 + 0.02 * sin(t * 0.13))) / (0.06 + 0.10 * uv.y), 2.0)) * (1.0 - uv.y) * 0.55;
    float beam_r = exp(-pow((uv.x - (0.80 + 0.02 * cos(t * 0.12))) / (0.06 + 0.10 * uv.y), 2.0)) * (1.0 - uv.y) * 0.45;
    /* A slow sweep across the floor. */
    float sweep = exp(-pow((uv.x - fract(t * 0.025) * 1.6 + 0.3) / 0.18, 2.0)) * smoothstep(horizon, 1.0, uv.y);

    vec3 c_cyan = vec3(0.05, 0.62, 1.00);
    vec3 c_lav = vec3(0.48, 0.36, 1.00);
    vec3 light = c_cyan * (cyan * 0.55 + mid * 0.30 + beam_l * 0.35 + sweep * 0.25) +
                 c_lav * (lav * 0.55 + beam_r * 0.35);
    col += light * 0.45;
    vec3 line_col = mix(vec3(0.10, 0.20, 0.55), vec3(0.30, 0.75, 1.0), clamp(cyan + mid + sweep, 0.0, 1.0));
    line_col = mix(line_col, vec3(0.55, 0.45, 1.0), clamp(lav, 0.0, 1.0) * 0.6);
    col += line_col * lines * (0.22 + 0.9 * clamp(cyan + lav + mid + sweep + 0.15, 0.0, 1.0));

    /* Vignette and dim (dim darkens for the launch screen). */
    vec2 vc = uv - 0.5;
    col *= 1.0 - dot(vc, vc) * 0.9;
    col *= 1.0 - pc.screen.w * 0.65;
    return vec4(col, 1.0);
}

vec4 glyph(vec2 p, float r)
{
    int subtype = int(v_p2.z + 0.5);
    float t = max(r * 0.085, 1.2);
    float d;
    if (subtype <= 3)
    {
        float ring = abs(length(p) - r * 0.90) - t;
        float inner;
        if (subtype == 0) /* cross */
        {
            float k = r * 0.36;
            inner = min(sdSegment(p, vec2(-k, -k), vec2(k, k)), sdSegment(p, vec2(-k, k), vec2(k, -k))) - t;
        }
        else if (subtype == 1) /* circle */
            inner = abs(length(p) - r * 0.42) - t;
        else if (subtype == 2) /* square */
            inner = abs(sdRoundBox(p, vec2(r * 0.38), r * 0.04)) - t;
        else /* triangle */
            inner = abs(sdTriangleUp(p + vec2(0.0, r * 0.05), r * 0.46)) - t;
        d = min(ring, inner);
    }
    else if (subtype == 4) /* filled arrow, rotated */
        d = sdTriangleUp(rotate(p, -v_p2.w), r * 0.85);
    else if (subtype == 5) /* outlined pointer triangle */
        d = abs(sdTriangleUp(p, r * 0.80)) - t * 1.3;
    else /* d-pad outline */
    {
        float arm = r * 0.30;
        float plus = min(sdRoundBox(p, vec2(r * 0.95, arm), r * 0.12), sdRoundBox(p, vec2(arm, r * 0.95), r * 0.12));
        d = abs(plus) - t;
    }
    return vec4(v_color.rgb, v_color.a * coverage(d));
}

vec3 spectrum(float h)
{
    return 0.5 + 0.5 * cos(6.28318 * (h + vec3(0.0, 0.33, 0.67)));
}

/* Thick GameCube-style glass.
 *  p2 = (reflection phase, side light, face, fade)
 *  face 0: the front of a block - tinted body, a thick edge that catches the
 *          light in shifting colours, a chrome rim, a bevel line, gloss and
 *          a slow reflection sweeping across.
 *  face 1: one slice of the block's thickness - only its outer ring is drawn,
 *          so stacked slices make the side walls around rounded corners.
 *  face 2: gloss alone, laid over a cover or an icon. */
vec4 glass(vec2 p, vec2 shape)
{
    int face = int(v_p2.z + 0.5);
    float t = pc.screen.z * (1.0 - pc.extra.x);
    float d = sdRoundBox(p, shape * 0.5, v_p0.y);
    float inside = coverage(d);
    vec2 n = p / (shape * 0.5);
    float yn = clamp(p.y / shape.y + 0.5, 0.0, 1.0);
    float ang = atan(p.y, p.x) / 6.28318;
    vec3 rainbow = spectrum(ang + v_p2.x * 1.3 + t * 0.012);

    if (face == 1)
    {
        float ring = coverage(-(d + 12.0)) * inside; /* the outer 12 px only */
        vec3 c = mix(v_color.rgb, mix(v_bcolor.rgb, rainbow, 0.65), 0.62) * v_p2.y;
        c += vec3(0.10) * (1.0 - yn);
        return vec4(c, v_color.a * ring * v_p2.w);
    }

    float edge_w = max(min(shape.x, shape.y) * 0.075, 6.0);
    float band = clamp(1.0 + d / edge_w, 0.0, 1.0);

    vec3 c = v_color.rgb * mix(1.18, 0.62, yn);
    float a = v_color.a;
    if (face == 2)
    {
        c = vec3(1.0);
        a = 0.0;
    }
    else
    {
        /* The thick edge, bright and dispersing. */
        float e = pow(band, 1.7) * 0.62;
        c = mix(c, mix(v_bcolor.rgb, rainbow, 0.5) * 1.15, e);
        a = mix(a, 0.95, e);
    }

    /* Gloss on the upper part, curved like a lens. */
    float gloss = smoothstep(0.46, 0.04, yn + n.x * n.x * 0.06) * 0.20;
    /* The room reflected: a soft diagonal sweep that slides as the block moves. */
    float s = n.x * 0.50 + n.y * 0.30 - v_p2.x * 1.6 + t * 0.035;
    float sweep = exp(-pow((fract(s * 0.5) - 0.5) / 0.065, 2.0)) * 0.26;
    float sweep2 = exp(-pow((fract(s * 0.5 + 0.11) - 0.5) / 0.02, 2.0)) * 0.12;
    float hl = (gloss + sweep + sweep2) * (face == 2 ? 0.9 : 1.0);
    if (face == 2)
        a = hl;
    else
    {
        c += vec3(hl);
        a = max(a, hl);
    }

    /* Bevel: a fine bright line just inside the rim, strongest at the top. */
    float bevel = coverage(abs(d + v_p0.z + 3.0) - 0.7) * (0.45 - 0.30 * yn);
    c = mix(c, vec3(1.0), bevel);
    a = max(a, bevel);

    vec4 o4 = vec4(c, a * inside);
    if (v_p0.z > 0.0)
    {
        /* Chrome rim: the border colour with a sliver of spectrum in it. */
        float b = coverage(abs(d + v_p0.z * 0.5) - v_p0.z * 0.5);
        vec3 rim = mix(v_bcolor.rgb, rainbow * 1.25, 0.42) + vec3(0.18) * (1.0 - yn);
        o4.rgb = mix(o4.rgb, rim, b * v_bcolor.a);
        o4.a = max(o4.a, b * v_bcolor.a);
    }
    if (v_p0.w > 0.0)
    {
        float q = max(d, 0.0) / v_p0.w;
        float g = exp(-q * q * 0.9) * step(0.0, d) * 0.6;
        o4.rgb = mix(v_bcolor.rgb, o4.rgb, o4.a / max(o4.a + g * v_bcolor.a, 1e-4));
        o4.a = max(o4.a, g * v_bcolor.a);
    }
    o4.a *= v_p2.w;
    return o4;
}

void main()
{
    int kind = int(v_p0.x + 0.5);
    vec2 quad = v_p1.xy;
    vec2 shape = v_p1.zw;
    vec2 p = (v_local - 0.5) * quad; /* pixels from the quad's centre */

    if (kind == K_BACKGROUND)
    {
        o = background();
        return;
    }
    if (kind == K_GLASS)
    {
        o = glass(p, shape);
        return;
    }
    if (kind == K_IMAGE)
    {
        o = texture(tex, v_uv) * v_color;
        return;
    }
    if (kind == K_TEXT)
    {
        float dist = texture(tex, v_uv).a;
        float edge = 0.5 - v_p0.y; /* p0.y > 0 makes the text heavier */
        float w = max(fwidth(dist) * 0.7, 1e-4);
        float a = smoothstep(edge - w, edge + w, dist);
        o = vec4(v_color.rgb, v_color.a * a);
        if (v_p0.w > 0.0) /* glow */
        {
            float g = smoothstep(edge - 0.25, edge, dist) * 0.55;
            o = vec4(mix(v_bcolor.rgb, v_color.rgb, a), max(o.a, g * v_bcolor.a));
        }
        return;
    }
    if (kind == K_GLYPH)
    {
        o = glyph(p, min(shape.x, shape.y) * 0.5);
        return;
    }
    if (kind == K_BLOB)
    {
        vec2 q = p / (shape * 0.5);
        o = vec4(v_color.rgb, v_color.a * exp(-dot(q, q) * 2.2));
        return;
    }
    if (kind == K_COVER || kind == K_REFLECTION)
    {
        float d = sdRoundBox(p, shape * 0.5, v_p0.y);
        vec4 c = texture(tex, v_uv) * v_color;
        float a = coverage(d);
        if (kind == K_REFLECTION)
            a *= pow(clamp(1.0 - v_local.y, 0.0, 1.0), 2.2);
        o = vec4(c.rgb, c.a * a);
        return;
    }

    /* K_PANEL: rounded rectangle with vertical gradient, sheen, border, glow. */
    float d = sdRoundBox(p, shape * 0.5, v_p0.y);
    float yn = clamp(p.y / shape.y + 0.5, 0.0, 1.0);
    vec3 fill = v_color.rgb * mix(1.0, v_p2.x, yn);
    float fill_a = v_color.a * coverage(d);
    /* Top sheen: a soft lighter band, as on glossy plastic. */
    fill += vec3(v_p2.y) * smoothstep(0.45, 0.0, yn) * 0.5;
    vec4 c = vec4(fill, fill_a);
    if (v_p0.z > 0.0)
    {
        float b = coverage(abs(d + v_p0.z * 0.5) - v_p0.z * 0.5);
        c.rgb = mix(c.rgb, v_bcolor.rgb, b * v_bcolor.a);
        c.a = max(c.a, b * v_bcolor.a);
    }
    if (v_p0.w > 0.0)
    {
        /* Gaussian falloff: gone well before the quad's edge (3x glow). */
        float q = max(d, 0.0) / v_p0.w;
        float g = exp(-q * q * 0.9) * step(0.0, d) * 0.6;
        c.rgb = mix(v_bcolor.rgb, c.rgb, c.a / max(c.a + g * v_bcolor.a, 1e-4));
        c.a = max(c.a, g * v_bcolor.a);
    }
    o = c;
}
