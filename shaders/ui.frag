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
    vec4 extra;  /* still background (0/1), background kind, colour filter, panel style */
    vec4 look0;  /* the theme's first light (rgb), w: its effect strength */
    vec4 look1;  /* the theme's second light (rgb), w: spare */
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
const int K_FX = 9; /* a full-screen overlay: p2.z picks it (1 tube, 2 terminal, 3 tape) */

/* Background kinds (pc.extra.y): the theme's room. */
const int BG_ROOM = 0;      /* Porpoise: the grid room */
const int BG_MIDNIGHT = 1;  /* black, for OLED screens */
const int BG_MINIMAL = 2;   /* a quiet gradient */
const int BG_CUBE = 3;      /* a glass cube turning over an endless grid */
const int BG_BROADCAST = 4; /* a tube TV between channels */
const int BG_TERMINAL = 5;  /* phosphor on black */
const int BG_DEPTH = 6;     /* a deep space of floating panes */
const int BG_AURORA = 7;    /* northern lights over a night sky */
const int BG_AERO = 8;      /* a bright sky with glossy bubbles */

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

float hash(vec2 q)
{
    return fract(sin(dot(q, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 q)
{
    vec2 i = floor(q), f = fract(q);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}

float fbm(vec2 q)
{
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; ++i)
    {
        s += a * noise(q);
        q = q * 2.03 + vec2(1.7, 9.2);
        a *= 0.5;
    }
    return s;
}

float now()
{
    return pc.screen.z * (1.0 - pc.extra.x);
}

/* Grid lines of cell size 1 at g, about one pixel wide. */
float grid_lines(vec2 g, float width)
{
    vec2 f = abs(fract(g + 0.5) - 0.5) / max(fwidth(g), vec2(1e-4));
    return 1.0 - min(min(f.x, f.y) / width, 1.0);
}

/* The grid room: a floor in perspective, a faint back wall, and slow light. */
vec3 room(vec2 uv)
{
    vec2 res = pc.screen.xy;
    float t = now();
    float aspect = res.x / res.y;
    float horizon = 0.53;
    vec3 c_cyan = pc.look0.rgb;
    vec3 c_lav = pc.look1.rgb;
    vec3 base = mix(vec3(0.010, 0.016, 0.040), c_cyan * 0.03 + c_lav * 0.02, 0.35);

    vec3 col = mix(base, base * 1.6 + vec3(0.0, 0.0, 0.005), smoothstep(0.0, horizon, uv.y));
    float lines = 0.0;

    if (uv.y > horizon)
    {
        float depth = 0.30 / (uv.y - horizon);
        vec2 g = vec2((uv.x - 0.5) * aspect * depth * 7.0, depth * 7.0 + t * 0.30);
        lines = grid_lines(g, 1.1) * 0.62 * exp(-depth * 0.10) * smoothstep(horizon, horizon + 0.05, uv.y);
        col += (c_cyan * 0.4 + c_lav * 0.3) * 0.07 * smoothstep(horizon, 1.0, uv.y);
    }
    else
    {
        vec2 g = vec2((uv.x - 0.5) * aspect * 16.0, (horizon - uv.y) * 16.0);
        lines = grid_lines(g, 1.1) * 0.30 * smoothstep(0.05, horizon, uv.y);
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

    vec3 light = c_cyan * (cyan * 0.55 + mid * 0.30 + beam_l * 0.35 + sweep * 0.25) +
                 c_lav * (lav * 0.55 + beam_r * 0.35);
    col += light * 0.45;
    vec3 line_dim = mix(c_cyan, c_lav, 0.5) * vec3(0.3, 0.3, 0.55);
    vec3 line_col = mix(line_dim, mix(c_cyan, vec3(1.0), 0.25), clamp(cyan + mid + sweep, 0.0, 1.0));
    line_col = mix(line_col, mix(c_lav, vec3(1.0), 0.15), clamp(lav, 0.0, 1.0) * 0.6);
    col += line_col * lines * (0.22 + 0.9 * clamp(cyan + lav + mid + sweep + 0.15, 0.0, 1.0));

    vec2 vc = uv - 0.5;
    col *= 1.0 - dot(vc, vc) * 0.9;
    return col;
}

/* OLED: true black, a whisper of the accent at the very bottom. */
vec3 midnight(vec2 uv)
{
    float t = now();
    float glow = exp(-pow((uv.y - 1.08) / 0.16, 2.0)) * (0.6 + 0.4 * sin(uv.x * 3.0 + t * 0.2));
    return pc.look0.rgb * glow * 0.10;
}

/* Minimal: two quiet tones and a soft light from the top left. */
vec3 minimal(vec2 uv)
{
    vec3 top = pc.look0.rgb, bottom = pc.look1.rgb;
    vec3 col = mix(top, bottom, smoothstep(0.0, 1.0, uv.y * 0.8 + uv.x * 0.2));
    float light = exp(-dot(uv - vec2(0.15, -0.1), uv - vec2(0.15, -0.1)) / 0.35);
    col += vec3(light * 0.035);
    col += (hash(floor(uv * pc.screen.xy)) - 0.5) * 0.006; /* a little grain, no banding */
    return col;
}

/* A rounded box's distance, for the cube. */
float sd_cube(vec3 p, float b, float r)
{
    vec3 q = abs(p) - vec3(b - r);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

mat3 turn(float yaw, float pitch)
{
    float cy = cos(yaw), sy = sin(yaw), cp = cos(pitch), sp = sin(pitch);
    return mat3(cy, 0, -sy, 0, 1, 0, sy, 0, cy) * mat3(1, 0, 0, 0, cp, sp, 0, -sp, cp);
}

/* Cube: black, an indigo grid floor that runs on forever, and a translucent
 * cube turning over it with light catching its edges. */
vec3 cube(vec2 uv)
{
    vec2 res = pc.screen.xy;
    float t = now();
    float aspect = res.x / res.y;
    vec3 violet = pc.look0.rgb, glow_c = pc.look1.rgb;
    vec3 col = vec3(0.006, 0.004, 0.016) + violet * 0.025 * (1.0 - uv.y);

    float horizon = 0.62;
    if (uv.y > horizon)
    {
        float depth = 0.25 / (uv.y - horizon);
        vec2 g = vec2((uv.x - 0.5) * aspect * depth * 5.0, depth * 5.0 + t * 0.18);
        float l = grid_lines(g, 1.2) * exp(-depth * 0.12) * smoothstep(horizon, horizon + 0.06, uv.y);
        col += mix(violet, glow_c, 0.3) * l * 0.55;
        col += violet * 0.06 * smoothstep(horizon, 1.0, uv.y);
    }
    /* The horizon line glows. */
    col += glow_c * exp(-pow((uv.y - horizon) / 0.006, 2.0)) * 0.35;

    /* The cube: large, far back and dim, its edges lit; ray-marched only
     * where its bounding sphere is. */
    vec2 q = vec2((uv.x - 0.5) * aspect, uv.y - 0.40);
    vec3 ro = vec3(0.0, 0.0, -5.0), rd = normalize(vec3(q, 1.6));
    float size = 1.05;
    float bsr = size * 1.75;
    float bq = dot(ro, rd), bc = dot(ro, ro) - bsr * bsr;
    if (bq * bq - bc > 0.0)
    {
        mat3 m = turn(t * 0.12 + 0.6, 0.62 + 0.12 * sin(t * 0.09));
        float dist = max(-bq - sqrt(bq * bq - bc), 0.0), hit = 0.0;
        vec3 pos = ro;
        for (int i = 0; i < 48; ++i)
        {
            pos = ro + rd * dist;
            float d = sd_cube(m * pos, size, 0.10);
            if (d < 0.002)
            {
                hit = 1.0;
                break;
            }
            dist += d;
            if (dist > 9.0)
                break;
        }
        if (hit > 0.0)
        {
            vec3 lp = m * pos;
            vec3 e = vec3(0.002, 0.0, 0.0);
            vec3 n = normalize(vec3(sd_cube(lp + e.xyy, size, 0.10) - sd_cube(lp - e.xyy, size, 0.10),
                                    sd_cube(lp + e.yxy, size, 0.10) - sd_cube(lp - e.yxy, size, 0.10),
                                    sd_cube(lp + e.yyx, size, 0.10) - sd_cube(lp - e.yyx, size, 0.10)));
            vec3 a = abs(lp) / size;
            float mx = max(a.x, max(a.y, a.z));
            float second = a.x + a.y + a.z - mx - min(a.x, min(a.y, a.z));
            float edges = smoothstep(0.80, 0.93, second);
            /* Faces: a soft grid of tiles, as if the cube were made of them. */
            vec2 fuv = (a.x >= mx - 1e-3) ? lp.yz : (a.y >= mx - 1e-3) ? lp.xz : lp.xy;
            float tiles = grid_lines(fuv / size * 2.0, 1.4) * 0.5;
            float light = clamp(dot(n, normalize(vec3(-0.4, -0.7, -0.6))), 0.0, 1.0);
            vec3 body = violet * (0.10 + 0.22 * light) + glow_c * tiles * 0.20;
            col = mix(col, body, 0.55);
            col += mix(glow_c, vec3(1.0), 0.35) * edges * 0.38;
        }
    }
    col += violet * exp(-dot(q, q) / 0.10) * 0.06;
    vec2 vc = uv - 0.5;
    col *= 1.0 - dot(vc, vc) * 0.8;
    return col;
}

/* Broadcast: a tube TV's glass, a little bent, between channels: deep blue
 * light, rolling bars and grain. */
vec2 bend(vec2 uv, float k)
{
    vec2 c = uv * 2.0 - 1.0;
    c *= 1.0 + k * dot(c, c) * vec2(0.6, 0.9);
    return c * 0.5 + 0.5;
}

vec3 broadcast(vec2 uv)
{
    float t = now();
    vec2 b = bend(uv, 0.06 * pc.look0.w);
    vec3 col = mix(pc.look0.rgb, pc.look1.rgb, smoothstep(0.0, 1.0, b.y));
    float roll = smoothstep(0.0, 0.25, abs(fract(b.y * 0.7 - t * 0.05) - 0.5));
    col *= 0.85 + 0.15 * roll;
    col += (hash(floor(b * pc.screen.xy * 0.5) + floor(t * 24.0)) - 0.5) * 0.035;
    vec2 vc = b - 0.5;
    col *= 1.0 - dot(vc, vc) * 1.4;
    return max(col, 0.0);
}

/* Terminal: phosphor green on black, the glow of the tube's middle. */
vec3 terminal(vec2 uv)
{
    vec2 vc = uv - 0.5;
    vec3 col = pc.look0.rgb * (0.035 * exp(-dot(vc, vc) * 2.5));
    vec2 cell = uv * pc.screen.xy / 24.0;
    vec2 f = abs(fract(cell) - 0.5);
    float dot_ = smoothstep(0.06, 0.0, length(f - 0.5) - 0.0); /* dots at the cell corners */
    col += pc.look0.rgb * dot_ * 0.03;
    return col;
}

/* Depth: panes of glass floating at many depths, drifting toward you. */
vec3 depth_space(vec2 uv)
{
    vec2 res = pc.screen.xy;
    float t = now();
    float aspect = res.x / res.y;
    vec2 p = vec2((uv.x - 0.5) * aspect, uv.y - 0.5);
    vec3 col = mix(vec3(0.004, 0.006, 0.02), pc.look1.rgb * 0.05, length(p));
    for (int layer = 0; layer < 6; ++layer)
    {
        float z = fract(float(layer) / 6.0 - t * 0.02); /* 0 far .. 1 near */
        float scale = mix(9.0, 1.4, z);
        vec2 g = p * scale + vec2(float(layer) * 7.3, float(layer) * 3.1);
        vec2 id = floor(g);
        vec2 f = fract(g) - 0.5;
        float r = hash(id + float(layer));
        if (r > 0.88)
        {
            float d = max(abs(f.x), abs(f.y)) - 0.28 - 0.1 * hash(id * 1.7);
            float rim = exp(-pow(d / (0.012 * scale), 2.0));
            float body = step(d, 0.0) * 0.12;
            float fade = smoothstep(0.0, 0.25, z) * smoothstep(1.0, 0.75, z);
            vec3 c = mix(pc.look0.rgb, pc.look1.rgb, hash(id + 3.0));
            col += c * (rim * 0.32 + body * 0.7) * fade * 0.45;
        }
    }
    /* A bright core far away that everything drifts out of. */
    col += pc.look0.rgb * exp(-dot(p, p) / 0.02) * 0.10;
    return col;
}

/* Aurora: ribbons of light over a starry night. */
vec3 aurora(vec2 uv)
{
    vec2 res = pc.screen.xy;
    float t = now();
    float aspect = res.x / res.y;
    vec3 col = mix(vec3(0.004, 0.010, 0.030), vec3(0.012, 0.030, 0.060), uv.y);
    float stars = step(0.9975, hash(floor(uv * res * 0.5))) * (0.5 + 0.5 * sin(t * 2.0 + hash(floor(uv * res)) * 30.0));
    col += vec3(stars) * 0.7 * (1.0 - uv.y);
    for (int i = 0; i < 3; ++i)
    {
        float fi = float(i);
        float x = uv.x * aspect * (1.2 + fi * 0.3) + fi * 2.1;
        float curve = 0.30 + 0.10 * fi + 0.10 * sin(x * 1.3 + t * 0.12 + fi) + 0.05 * fbm(vec2(x * 1.5, t * 0.05 + fi));
        float band = exp(-pow((uv.y - curve) / (0.05 + 0.03 * fi), 2.0));
        float rays = 0.5 + 0.5 * fbm(vec2(x * 6.0, t * 0.1 + fi * 4.0));
        float above = smoothstep(curve + 0.02, curve - 0.25, uv.y);
        vec3 c = mix(pc.look0.rgb, pc.look1.rgb, fi / 2.0);
        col += c * (band * 0.55 + above * band * 0.0 + exp(-pow((uv.y - curve + 0.12) / 0.14, 2.0)) * 0.12 * rays) *
               rays;
    }
    /* A dark hill line along the bottom. */
    float hill = 0.90 + 0.04 * sin(uv.x * 5.0) + 0.02 * sin(uv.x * 13.0 + 1.0);
    col = mix(col, vec3(0.002, 0.004, 0.010), smoothstep(hill - 0.003, hill + 0.003, uv.y));
    return col;
}

/* Aero: a bright sky, a glossy wave of light and soft bubbles. */
vec3 aero(vec2 uv)
{
    vec2 res = pc.screen.xy;
    float t = now();
    float aspect = res.x / res.y;
    vec3 col = mix(pc.look0.rgb, pc.look1.rgb, smoothstep(0.0, 1.0, uv.y));
    /* Two glossy swooshes across the lower half. */
    for (int i = 0; i < 2; ++i)
    {
        float fi = float(i);
        float y = 0.72 + fi * 0.08 + 0.06 * sin(uv.x * 3.0 + t * 0.15 + fi * 2.0);
        float d = uv.y - y;
        col += vec3(1.0) * exp(-pow(d / 0.012, 2.0)) * 0.25;
        col += vec3(1.0) * smoothstep(0.0, -0.12, d) * smoothstep(-0.25, -0.12, d) * 0.05;
    }
    /* Bubbles rising slowly. */
    vec2 p = vec2(uv.x * aspect, uv.y);
    for (int i = 0; i < 9; ++i)
    {
        float fi = float(i);
        vec2 c = vec2(hash(vec2(fi, 1.0)) * aspect, 1.15 - fract(hash(vec2(fi, 2.0)) + t * (0.010 + 0.006 * hash(vec2(fi, 3.0)))) * 1.4);
        float r = 0.03 + 0.07 * hash(vec2(fi, 4.0));
        float d = length(p - c) - r;
        float ring = exp(-pow(d / 0.004, 2.0)) * 0.35 + smoothstep(0.0, -r, d) * 0.06;
        float spec = exp(-dot(p - c + vec2(r * 0.4, r * 0.45), p - c + vec2(r * 0.4, r * 0.45)) / (r * r * 0.04)) * 0.5;
        col += vec3(1.0) * (ring + spec);
    }
    return col;
}

vec3 background_at(vec2 uv)
{
    int kind = int(pc.extra.y + 0.5);
    vec3 col;
    if (kind == BG_MIDNIGHT)
        col = midnight(uv);
    else if (kind == BG_MINIMAL)
        col = minimal(uv);
    else if (kind == BG_CUBE)
        col = cube(uv);
    else if (kind == BG_BROADCAST)
        col = broadcast(uv);
    else if (kind == BG_TERMINAL)
        col = terminal(uv);
    else if (kind == BG_DEPTH)
        col = depth_space(uv);
    else if (kind == BG_AURORA)
        col = aurora(uv);
    else if (kind == BG_AERO)
        col = aero(uv);
    else
        col = room(uv);
    /* Dim darkens for the launch screen. */
    return col * (1.0 - pc.screen.w * 0.65);
}

vec4 background()
{
    return vec4(background_at(v_local), 1.0);
}

/* A five-pointed star (after Inigo Quilez's sdStar5), point up on screen. */
float sdStar5(vec2 p, float r, float rf)
{
    const vec2 k1 = vec2(0.809016994375, -0.587785252292);
    const vec2 k2 = vec2(-k1.x, k1.y);
    p.y = -p.y;
    p.x = abs(p.x);
    p -= 2.0 * max(dot(k1, p), 0.0) * k1;
    p -= 2.0 * max(dot(k2, p), 0.0) * k2;
    p.x = abs(p.x);
    p.y -= r;
    vec2 ba = rf * vec2(-k1.y, k1.x) - vec2(0.0, 1.0);
    float h = clamp(dot(p, ba) / dot(ba, ba), 0.0, r);
    return length(p - ba * h) * sign(p.y * ba.x - p.x * ba.y);
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
    else if (subtype == 7) /* filled star */
        d = sdStar5(p + vec2(0.0, -r * 0.06), r * 0.92, 0.45) - r * 0.04;
    else if (subtype == 8) /* outlined star */
        d = abs(sdStar5(p + vec2(0.0, -r * 0.06), r * 0.88, 0.45)) - t;
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
    rainbow = mix(rainbow, vec3(0.85), pc.look1.w); /* plain themes: clear glass, no spectrum */

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

/* Liquid glass (pc.extra.w == 1): a pane that bends the room behind it at
 * its rounded edges, splitting the light there into a thin spectrum, with a
 * bright rim on top, a soft shadow beneath and a tint of the panel's colour.
 * Only panels with a fill; outlines stay plain. */
vec4 liquid_panel(vec2 p, vec2 shape, vec2 quad)
{
    float r = v_p0.y;
    vec2 b = shape * 0.5;
    float d = sdRoundBox(p, b, r);
    float inside = coverage(d);
    float fill_a = v_color.a;

    /* The shadow it casts: soft, a little below. */
    float ds = sdRoundBox(p - vec2(0.0, min(shape.y * 0.10, 10.0)), b, r);
    float spread = clamp(min(shape.x, shape.y) * 0.22, 6.0, 22.0);
    float shadow = (1.0 - smoothstep(-spread * 0.4, spread, ds)) * (1.0 - inside) * 0.38 * min(fill_a * 1.4, 1.0);

    /* Where it bends: within a band inside the rim, along the outward normal. */
    vec2 inner = clamp(p, -(b - r), b - r);
    vec2 n = p - inner;
    float nl = length(n);
    n = nl > 1e-3 ? n / nl : vec2(0.0, sign(p.y + 1e-3));
    float band_w = clamp(min(shape.x, shape.y) * 0.32, 8.0, 34.0);
    float band = clamp(1.0 + d / band_w, 0.0, 1.0); /* 1 at the rim, 0 deeper in */
    float bend = band * band * band;

    vec2 res = pc.screen.xy;
    vec2 suv = gl_FragCoord.xy / res;
    /* How far the room is pulled, in screen units: small pieces bend it less. */
    float k = bend * 26.0 / res.y * mix(0.25, 1.0, smoothstep(50.0, 220.0, min(shape.x, shape.y)));
    vec2 dir = -n * vec2(res.y / res.x, 1.0);
    vec3 behind;
    behind.r = background_at(suv + dir * k * 1.00).r;
    behind.g = background_at(suv + dir * k * 1.35).g;
    behind.b = background_at(suv + dir * k * 1.75).b;
    /* A little frosting: the room lifted toward the tint, never a flat colour. */
    vec3 tint = v_color.rgb;
    float yn = clamp(p.y / shape.y + 0.5, 0.0, 1.0);
    vec3 body = mix(behind * 1.12 + 0.025, tint, clamp(0.30 + fill_a * 0.50, 0.0, 0.9));
    body *= mix(1.10, 0.86, yn); /* lighter above, as light falls from the top */

    /* The rim: brightest where it faces up, with a sliver of spectrum that
     * is strongest at the ends, where the glass curves most. */
    float rim_line = coverage(abs(d + 0.9) - 0.9);
    float facing = clamp(-n.y * 0.6 + 0.55, 0.0, 1.0);
    float ang = atan(n.y, n.x) / 6.28318;
    vec3 rainbow = spectrum(ang * 2.0 + p.x / max(shape.x, 1.0) * 0.6 + now() * 0.01);
    float ends = clamp(abs(n.x) * 1.4, 0.0, 1.0);
    vec3 rim_c = mix(vec3(1.0), rainbow, 0.25 + 0.45 * ends);
    body += rim_c * bend * (0.16 + 0.22 * facing);
    body = mix(body, rim_c, rim_line * (0.40 + 0.50 * facing));
    /* A crisp highlight just inside the top edge, and a softer one below it. */
    float spec_line = coverage(abs(d + 3.2) - 0.8) * facing * facing * smoothstep(0.55, 0.0, yn);
    body = mix(body, vec3(1.0), spec_line * 0.55);
    float sheen = smoothstep(0.50, 0.0, yn) * 0.10;
    body += vec3(sheen);
    /* The inner shadow along the bottom edge gives it thickness. */
    body *= 1.0 - band * smoothstep(0.55, 1.0, yn) * 0.30;
    vec4 c = vec4(body, inside * mix(0.86, 0.97, fill_a));
    if (v_p0.z > 0.0)
    {
        float bline = coverage(abs(d + v_p0.z * 0.5) - v_p0.z * 0.5);
        vec3 bc = mix(v_bcolor.rgb, rim_c, 0.25);
        c.rgb = mix(c.rgb, bc, bline * v_bcolor.a * 0.85);
    }
    if (v_p0.w > 0.0)
    {
        float q = max(d, 0.0) / v_p0.w;
        float g = exp(-q * q * 0.9) * step(0.0, d) * 0.6;
        c.rgb = mix(v_bcolor.rgb, c.rgb, c.a / max(c.a + g * v_bcolor.a, 1e-4));
        c.a = max(c.a, g * v_bcolor.a);
    }
    /* Shadow under everything else. */
    float sa = shadow * (1.0 - c.a);
    c.rgb = (c.rgb * c.a) / max(c.a + sa, 1e-4);
    c.a = c.a + sa;
    return c;
}

/* Full-screen overlays a theme lays over everything. */
vec4 fx(vec2 uv)
{
    int which = int(v_p2.z + 0.5);
    float t = now();
    float y = gl_FragCoord.y;
    if (which == 1 || which == 3)
    {
        /* A tube: scanlines, the glass's dark edges and, for tape, a band of
         * tracking noise rolling down. */
        float scan = 0.5 + 0.5 * cos(6.28318 * y / 3.0);
        float a = scan * 0.16 * v_color.a;
        vec2 vc = uv - 0.5;
        a += smoothstep(0.18, 0.55, dot(vc, vc)) * 0.45;
        vec3 c = vec3(0.0);
        if (which == 3)
        {
            /* Now and then (a couple of seconds in every twelve) the tape's
             * tracking slips: a thin band of sparks rolls down. */
            float cycle = fract(t / 12.0);
            float on = smoothstep(0.0, 0.02, cycle) * smoothstep(0.20, 0.17, cycle);
            float band = exp(-pow((fract(uv.y - cycle * 5.0) - 0.5) / 0.006, 2.0)) * on;
            float nz = hash(vec2(floor(gl_FragCoord.x * 0.5), floor(y * 0.5)) + floor(t * 30.0));
            float spark = band * step(0.6, nz) * 0.40;
            a = max(a, spark);
            return vec4(mix(vec3(0.0), vec3(1.0), spark / max(a, 1e-4)), a);
        }
        return vec4(c, a);
    }
    if (which == 2)
    {
        /* Terminal: fine scanlines and a slow refresh line. */
        float scan = 0.5 + 0.5 * cos(6.28318 * y / 2.0);
        float sweep = exp(-pow((fract(uv.y * 0.5 - t * 0.04) - 0.5) / 0.03, 2.0)) * 0.05;
        vec4 o4 = vec4(0.0, 0.0, 0.0, scan * 0.18 * v_color.a);
        o4.rgb = pc.look0.rgb;
        o4.a = max(o4.a, sweep);
        o4.rgb = mix(vec3(0.0), pc.look0.rgb, sweep / max(o4.a, 1e-4));
        return o4;
    }
    return vec4(0.0);
}

/* Colour filters for colour blindness (pc.extra.z: 1 red-weak, 2 green-weak,
 * 3 blue-weak, 4 greyscale): what a viewer would miss is moved into the
 * channels they see well (daltonising, after Fidaner et al.), so reds and
 * greens, or blues and yellows, stay apart. */
vec3 colour_filter(vec3 c)
{
    int mode = int(pc.extra.z + 0.5);
    if (mode == 0)
        return c;
    if (mode == 4)
        return vec3(dot(c, vec3(0.299, 0.587, 0.114)));
    /* To LMS, simulate the deficiency, back. */
    mat3 rgb2lms = mat3(17.8824, 3.45565, 0.0299566, 43.5161, 27.1554, 0.184309, 4.11935, 3.86714, 1.46709);
    mat3 lms2rgb = mat3(0.0809444479, -0.0102485335, -0.000365296938, -0.130504409, 0.0540193266, -0.00412161469,
                        0.116721066, -0.113614708, 0.693511405);
    vec3 lms = rgb2lms * c;
    vec3 sim;
    if (mode == 1)
        sim = vec3(2.02344 * lms.y - 2.52581 * lms.z, lms.y, lms.z);
    else if (mode == 2)
        sim = vec3(lms.x, 0.494207 * lms.x + 1.24827 * lms.z, lms.z);
    else
        sim = vec3(lms.x, lms.y, -0.395913 * lms.x + 0.801109 * lms.y);
    vec3 seen = lms2rgb * sim;
    vec3 err = c - seen;
    vec3 shift = vec3(0.0, err.r * 0.7 + err.g, err.r * 0.7 + err.b);
    return clamp(c + shift, 0.0, 1.0);
}

vec4 shade()
{
    int kind = int(v_p0.x + 0.5);
    vec2 quad = v_p1.xy;
    vec2 shape = v_p1.zw;
    vec2 p = (v_local - 0.5) * quad; /* pixels from the quad's centre */

    if (kind == K_BACKGROUND)
        return background();
    if (kind == K_GLASS)
        return glass(p, shape);
    if (kind == K_FX)
        return fx(v_local);
    if (kind == K_IMAGE)
        return texture(tex, v_uv) * v_color;
    if (kind == K_TEXT)
    {
        float dist = texture(tex, v_uv).a;
        float edge = 0.5 - v_p0.y; /* p0.y > 0 makes the text heavier */
        float w = max(fwidth(dist) * 0.7, 1e-4);
        float a = smoothstep(edge - w, edge + w, dist);
        vec4 o4 = vec4(v_color.rgb, v_color.a * a);
        if (v_p0.w > 0.0) /* glow */
        {
            float g = smoothstep(edge - 0.25, edge, dist) * 0.55;
            o4 = vec4(mix(v_bcolor.rgb, v_color.rgb, a), max(o4.a, g * v_bcolor.a));
        }
        return o4;
    }
    if (kind == K_GLYPH)
        return glyph(p, min(shape.x, shape.y) * 0.5);
    if (kind == K_BLOB)
    {
        vec2 q = p / (shape * 0.5);
        return vec4(v_color.rgb, v_color.a * exp(-dot(q, q) * 2.2));
    }
    if (kind == K_COVER || kind == K_REFLECTION)
    {
        float d = sdRoundBox(p, shape * 0.5, v_p0.y);
        vec4 c = texture(tex, v_uv) * v_color;
        float a = coverage(d);
        if (kind == K_REFLECTION)
            a *= pow(clamp(1.0 - v_local.y, 0.0, 1.0), 2.2);
        return vec4(c.rgb, c.a * a);
    }

    /* K_PANEL: rounded rectangle with vertical gradient, sheen, border, glow. */
    if (int(pc.extra.w + 0.5) == 1 && v_color.a > 0.05 && v_p2.z < 0.5)
        return liquid_panel(p, shape, quad);
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
    return c;
}

void main()
{
    o = shade();
    o.rgb = colour_filter(o.rgb);
}
