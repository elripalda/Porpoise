#version 450
/* Porpoise: the game's picture on its way to the TV, with an optional screen
 * filter. Copyright (C) 2026 Ruben (Project Porpoise), GPL-3.0-or-later.
 *
 * params: x filter (0 smooth, 1 sharp, 2 sharpen, 3 CRT, 4 arcade CRT, 5 VHS,
 *           6 soft VHS, 7 8-bit, 8 pocket LCD, 9 scanlines, 10 shadow mask,
 *           11 LCD, 12 FSR 1),
 *         y strength 0..1, z time in seconds, w colour filter (0 off,
 *         1 red-weak, 2 green-weak, 3 blue-weak, 4 greyscale: Accessibility).
 * size:   x, y the picture's texture size in texels; z, w its size on screen
 *         in pixels. Smooth and sharp differ only in the sampler the host
 *         binds; the others are worked out here. */
layout(push_constant) uniform Push {
    vec4 rect;
    vec4 uv;
    vec4 color;
    vec4 params;
    vec4 size;
} p;
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 frag;

float hash(vec2 q)
{
    return fract(sin(dot(q, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 sample_rgb(vec2 uv)
{
    return texture(tex, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
}

/* A light contrast-adaptive sharpen (after AMD's CAS idea): pull the centre
 * away from the average of its neighbours, less where it is already sharp. */
vec3 sharpen(vec2 uv, float amount)
{
    vec2 t = 1.0 / p.size.xy;
    vec3 c = sample_rgb(uv);
    vec3 n = sample_rgb(uv + vec2(0.0, -t.y));
    vec3 s = sample_rgb(uv + vec2(0.0, t.y));
    vec3 e = sample_rgb(uv + vec2(t.x, 0.0));
    vec3 w = sample_rgb(uv + vec2(-t.x, 0.0));
    vec3 mn = min(c, min(min(n, s), min(e, w)));
    vec3 mx = max(c, max(max(n, s), max(e, w)));
    vec3 room = min(mn, 1.0 - mx) / max(mx, vec3(1e-4));
    vec3 wgt = -sqrt(clamp(room, 0.0, 1.0)) * mix(0.08, 0.2, amount);
    return clamp((c + (n + s + e + w) * wgt) / (1.0 + 4.0 * wgt), 0.0, 1.0);
}

/* Scanlines over the game's own lines, and a soft glow so bright lines bloom
 * into the dark ones, as a tube does. */
vec3 crt(vec2 uv, float amount, float lines, float mask_amount)
{
    vec3 c = sample_rgb(uv);
    vec2 t = 1.0 / p.size.xy;
    vec3 glow = (sample_rgb(uv + vec2(t.x * 2.0, 0.0)) + sample_rgb(uv - vec2(t.x * 2.0, 0.0)) +
                 sample_rgb(uv + vec2(0.0, t.y * 2.0)) + sample_rgb(uv - vec2(0.0, t.y * 2.0))) * 0.25;
    float y = uv.y * lines;
    float beam = 0.5 + 0.5 * cos(6.2831853 * y);           /* 1 on a line, 0 between */
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    float width = mix(0.35, 0.75, lum);                     /* bright lines are fatter */
    float scan = mix(1.0, smoothstep(0.0, width, beam) * 0.85 + 0.15, amount);
    vec3 col = c * scan + glow * 0.18 * amount;
    /* An aperture grille: every third screen pixel favours red, green, blue. */
    int column = int(gl_FragCoord.x) % 3;
    vec3 mask = column == 0 ? vec3(1.0, 0.7, 0.7) : column == 1 ? vec3(0.7, 1.0, 0.7) : vec3(0.7, 0.7, 1.0);
    col *= mix(vec3(1.0), mask, mask_amount * amount);
    return col * (1.0 + 0.25 * amount); /* win back the light the lines took */
}

vec3 vhs(vec2 uv, float amount)
{
    float time = p.params.z;
    /* The picture wobbles sideways a little, more in a band that rolls down
     * the screen, the way a worn tape's tracking does. */
    float band = smoothstep(0.0, 0.04, abs(fract(uv.y - time * 0.07) - 0.5) - 0.44);
    float wobble = sin(uv.y * 40.0 + time * 3.0) * 0.0009 + (1.0 - band) * 0.004 * sin(time * 47.0 + uv.y * 300.0);
    vec2 u = uv + vec2(wobble * amount, 0.0);
    /* Colour smears to the right of the brightness (chroma bleed). */
    vec2 t = vec2(1.0 / p.size.x, 0.0);
    float shift = mix(1.5, 5.0, amount);
    vec3 luma_src = (sample_rgb(u - t) + sample_rgb(u) * 2.0 + sample_rgb(u + t)) * 0.25;
    vec3 chroma_src = (sample_rgb(u + t * shift) + sample_rgb(u + t * shift * 2.0)) * 0.5;
    float y = dot(luma_src, vec3(0.299, 0.587, 0.114));
    vec3 chroma = chroma_src - dot(chroma_src, vec3(0.299, 0.587, 0.114));
    vec3 col = vec3(y) + chroma * mix(1.0, 0.75, amount);
    /* Noise, a little more in the rolling band, and soft scanlines. */
    float n = hash(floor(gl_FragCoord.xy * vec2(0.5, 1.0)) + fract(time) * 100.0) - 0.5;
    col += n * mix(0.03, 0.09, amount) * (1.0 + (1.0 - band) * 2.0);
    col *= 0.94 + 0.06 * cos(6.2831853 * uv.y * 240.0);
    /* Tape white is never quite white, nor black quite black. */
    col = mix(col, col * vec3(1.0, 0.97, 0.92) + 0.02, 0.6 * amount);
    return clamp(col, 0.0, 1.0);
}


float luma(vec3 c)
{
    return dot(c, vec3(0.299, 0.587, 0.114));
}

/* A clean tape: soft, a little smeared, faded and warm, with a gentle glow
 * round the bright parts. No noise, no tracking. */
vec3 soft_vhs(vec2 uv, float amount)
{
    vec2 t = vec2(1.0 / p.size.x, 0.0);
    vec2 v = vec2(0.0, 1.0 / p.size.y);
    float spread = mix(1.0, 2.2, amount);
    vec3 c = sample_rgb(uv - t * 2.0 * spread) * 0.1 + sample_rgb(uv - t * spread) * 0.2 + sample_rgb(uv) * 0.4 +
             sample_rgb(uv + t * spread) * 0.2 + sample_rgb(uv + t * 2.0 * spread) * 0.1;
    vec3 shifted = sample_rgb(uv + t * mix(1.5, 4.0, amount));
    vec3 col = vec3(luma(c)) + (shifted - luma(shifted)) * 0.9;
    vec3 wide = (sample_rgb(uv + t * 6.0) + sample_rgb(uv - t * 6.0) + sample_rgb(uv + v * 4.0) +
                 sample_rgb(uv - v * 4.0)) * 0.25;
    col += max(wide - 0.5, 0.0) * mix(0.3, 0.8, amount);
    col = mix(col, vec3(luma(col)), 0.22 * amount);
    col = col * mix(0.94, 0.84, amount) + mix(0.03, 0.08, amount);
    col *= mix(vec3(1.0), vec3(1.05, 1.0, 0.9), amount);
    col *= 0.975 + 0.025 * cos(6.2831853 * uv.y * 240.0);
    return clamp(col, 0.0, 1.0);
}

/* Big pixels, few colours, an ordered dither: a home console of the 80s. */
vec3 eight_bit(vec2 uv, float amount)
{
    float across = mix(320.0, 128.0, amount);
    vec2 cells = vec2(across, across * p.size.w / max(p.size.z, 1.0));
    vec2 cell = floor(uv * cells);
    vec3 c = sample_rgb((cell + 0.5) / cells);
    const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0,
                                      13.0, 5.0);
    int bx = int(mod(cell.x, 4.0)), by = int(mod(cell.y, 4.0));
    float d = (bayer[by * 4 + bx] + 0.5) / 16.0 - 0.5;
    float levels = mix(6.0, 3.0, amount);
    c = floor(c * (levels - 1.0) + 0.5 + d) / (levels - 1.0);
    return clamp(c, 0.0, 1.0);
}

/* A handheld's green screen: four shades, and the grid between its pixels. */
vec3 pocket(vec2 uv, float amount)
{
    float across = mix(240.0, 160.0, amount);
    vec2 cells = vec2(across, across * p.size.w / max(p.size.z, 1.0));
    vec2 cell = floor(uv * cells);
    float y = smoothstep(0.06, 0.8, luma(sample_rgb((cell + 0.5) / cells)));
    const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0,
                                      13.0, 5.0);
    float d = (bayer[int(mod(cell.y, 4.0)) * 4 + int(mod(cell.x, 4.0))] + 0.5) / 16.0 - 0.5;
    float shade = floor(clamp(y * 3.0 + 0.5 + d * 0.9, 0.0, 3.999));
    vec3 col = shade < 1.0 ? vec3(0.06, 0.22, 0.06)
             : shade < 2.0 ? vec3(0.19, 0.38, 0.19)
             : shade < 3.0 ? vec3(0.55, 0.67, 0.06)
                           : vec3(0.61, 0.74, 0.06);
    vec2 f = fract(uv * cells);
    float grid = smoothstep(0.0, 0.12, f.x) * smoothstep(0.0, 0.12, f.y);
    return mix(col * 0.82, col, grid) * mix(1.0, 0.95, amount);
}

/* Plain scanlines, as on a 240-line set: dark gaps between the game's lines
 * and nothing else (no glow, no mask). */
vec3 scanlines(vec2 uv, float amount)
{
    vec3 c = sample_rgb(uv);
    float beam = 0.5 + 0.5 * cos(6.2831853 * uv.y * 240.0); /* 1 on a line, 0 between */
    float lum = luma(c);
    float line = smoothstep(0.0, mix(0.45, 0.8, lum), beam);
    return clamp(c * mix(1.0, 0.25 + 0.75 * line, amount) * (1.0 + 0.3 * amount), 0.0, 1.0);
}

/* A TV's shadow mask: phosphor dots in staggered triads, the beam's lines, and
 * a glow round the bright parts. Finer than Arcade CRT, with no bend. */
vec3 shadow_mask(vec2 uv, float amount)
{
    vec3 col = crt(uv, amount * 0.7, 480.0, 0.0);
    ivec2 px = ivec2(gl_FragCoord.xy);
    int row = (px.y / 2) % 2;
    int column = (px.x + row * 2) % 3; /* every other pair of rows shifts: the triads stagger */
    vec3 dots = column == 0 ? vec3(1.0, 0.45, 0.45) : column == 1 ? vec3(0.45, 1.0, 0.45) : vec3(0.45, 0.45, 1.0);
    float gap = (px.y % 2 == 1) ? mix(1.0, 0.82, amount) : 1.0; /* the dark between dot rows */
    col *= mix(vec3(1.0), dots, 0.65 * amount) * gap;
    return clamp(col * (1.0 + 0.45 * amount), 0.0, 1.0);
}

/* A sharp LCD: the game's own pixels (about 640 across) kept crisp, each made
 * of red, green and blue stripes with a thin dark gap round it. */
vec3 lcd(vec2 uv, float amount)
{
    vec2 cells = vec2(640.0, 640.0 * p.size.w / max(p.size.z, 1.0));
    vec2 cell = floor(uv * cells);
    vec2 f = fract(uv * cells);
    vec3 c = sample_rgb((cell + 0.5) / cells); /* sharp: each cell one colour */
    float sub = f.x * 3.0;
    vec3 stripe = sub < 1.0 ? vec3(1.0, 0.55, 0.55) : sub < 2.0 ? vec3(0.55, 1.0, 0.55) : vec3(0.55, 0.55, 1.0);
    float pixel_px = p.size.z / cells.x; /* screen pixels per cell */
    float stripes = smoothstep(2.0, 5.0, pixel_px); /* only where the stripes can be seen */
    float grid = smoothstep(0.0, 0.1, f.x) * smoothstep(0.0, 0.1, f.y) * smoothstep(1.0, 0.92, f.y);
    vec3 col = c * mix(vec3(1.0), stripe, 0.5 * amount * stripes);
    col *= mix(1.0, mix(0.55, 1.0, grid), amount * smoothstep(1.5, 3.0, pixel_px));
    return clamp(col * (1.0 + 0.3 * amount), 0.0, 1.0);
}

/* FSR 1's upscaler (EASU), after AMD FidelityFX Super Resolution 1.0
 * (ffx_fsr1.h, MIT licence, Copyright (c) 2021 Advanced Micro Devices, Inc.):
 * twelve of the game's pixels round the spot, weighted along the edge they
 * make, so edges stay clean when a picture is scaled up to the TV. */
vec3 easu_tex(ivec2 q)
{
    return texelFetch(tex, clamp(q, ivec2(0), ivec2(p.size.xy) - 1), 0).rgb;
}

float easu_luma(vec3 c)
{
    return c.b * 0.5 + (c.r * 0.5 + c.g);
}

void easu_set(inout vec2 dir, inout float len, float w, float la, float lb, float lc, float ld, float le)
{
    float dc = ld - lc, cb = lc - lb;
    float len_x = max(abs(dc), abs(cb));
    len_x = len_x > 0.0 ? 1.0 / len_x : 0.0;
    float dir_x = ld - lb;
    dir.x += dir_x * w;
    len_x = clamp(abs(dir_x) * len_x, 0.0, 1.0);
    len += len_x * len_x * w;
    float ec = le - lc, ca = lc - la;
    float len_y = max(abs(ec), abs(ca));
    len_y = len_y > 0.0 ? 1.0 / len_y : 0.0;
    float dir_y = le - la;
    dir.y += dir_y * w;
    len_y = clamp(abs(dir_y) * len_y, 0.0, 1.0);
    len += len_y * len_y * w;
}

void easu_tap(inout vec3 ac, inout float aw, vec2 off, vec2 dir, vec2 len2, float lob, float clp, vec3 c)
{
    vec2 v = vec2(off.x * dir.x + off.y * dir.y, off.x * -dir.y + off.y * dir.x) * len2;
    float d2 = min(v.x * v.x + v.y * v.y, clp);
    float wb = 0.4 * d2 - 1.0;
    float wa = lob * d2 - 1.0;
    wb *= wb;
    wa *= wa;
    wb = 1.5625 * wb - 0.5625;
    float w = wb * wa;
    ac += c * w;
    aw += w;
}

vec3 fsr_easu(vec2 uv)
{
    vec2 pp = uv * p.size.xy - 0.5;
    vec2 fp = floor(pp);
    pp -= fp;
    ivec2 q = ivec2(fp);
    /*    b c
     *  e f g h
     *  i j k l
     *    n o    */
    vec3 b = easu_tex(q + ivec2(0, -1)), c = easu_tex(q + ivec2(1, -1));
    vec3 e = easu_tex(q + ivec2(-1, 0)), f = easu_tex(q), g = easu_tex(q + ivec2(1, 0)),
         h = easu_tex(q + ivec2(2, 0));
    vec3 i = easu_tex(q + ivec2(-1, 1)), j = easu_tex(q + ivec2(0, 1)), k = easu_tex(q + ivec2(1, 1)),
         l = easu_tex(q + ivec2(2, 1));
    vec3 n = easu_tex(q + ivec2(0, 2)), o = easu_tex(q + ivec2(1, 2));
    float bl = easu_luma(b), cl = easu_luma(c), el = easu_luma(e), fl = easu_luma(f), gl = easu_luma(g),
          hl = easu_luma(h), il = easu_luma(i), jl = easu_luma(j), kl = easu_luma(k), ll = easu_luma(l),
          nl = easu_luma(n), ol = easu_luma(o);
    vec2 dir = vec2(0.0);
    float len = 0.0;
    easu_set(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bl, el, fl, gl, jl);
    easu_set(dir, len, pp.x * (1.0 - pp.y), cl, fl, gl, hl, kl);
    easu_set(dir, len, (1.0 - pp.x) * pp.y, fl, il, jl, kl, nl);
    easu_set(dir, len, pp.x * pp.y, gl, jl, kl, ll, ol);
    vec2 dir2 = dir * dir;
    float dir_r = dir2.x + dir2.y;
    bool zero = dir_r < 1.0 / 32768.0;
    dir_r = zero ? 1.0 : inversesqrt(dir_r);
    dir.x = zero ? 1.0 : dir.x;
    dir *= dir_r;
    len = len * 0.5;
    len *= len;
    float stretch = (dir.x * dir.x + dir.y * dir.y) / max(abs(dir.x), abs(dir.y));
    vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
    float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
    float clp = 1.0 / lob;
    vec3 ac = vec3(0.0);
    float aw = 0.0;
    easu_tap(ac, aw, vec2(0.0, -1.0) - pp, dir, len2, lob, clp, b);
    easu_tap(ac, aw, vec2(1.0, -1.0) - pp, dir, len2, lob, clp, c);
    easu_tap(ac, aw, vec2(-1.0, 1.0) - pp, dir, len2, lob, clp, i);
    easu_tap(ac, aw, vec2(0.0, 1.0) - pp, dir, len2, lob, clp, j);
    easu_tap(ac, aw, vec2(0.0, 0.0) - pp, dir, len2, lob, clp, f);
    easu_tap(ac, aw, vec2(-1.0, 0.0) - pp, dir, len2, lob, clp, e);
    easu_tap(ac, aw, vec2(1.0, 1.0) - pp, dir, len2, lob, clp, k);
    easu_tap(ac, aw, vec2(2.0, 1.0) - pp, dir, len2, lob, clp, l);
    easu_tap(ac, aw, vec2(2.0, 0.0) - pp, dir, len2, lob, clp, h);
    easu_tap(ac, aw, vec2(1.0, 0.0) - pp, dir, len2, lob, clp, g);
    easu_tap(ac, aw, vec2(1.0, 2.0) - pp, dir, len2, lob, clp, o);
    easu_tap(ac, aw, vec2(0.0, 2.0) - pp, dir, len2, lob, clp, n);
    /* No ringing: the result stays within the four nearest pixels. */
    vec3 mn = min(min(f, g), min(j, k));
    vec3 mx = max(max(f, g), max(j, k));
    return clamp(min(mx, max(mn, ac / max(aw, 1e-6))), 0.0, 1.0);
}

/* As the menus' (shaders/ui.frag): daltonising after Fidaner et al. */
vec3 colour_filter(vec3 c, int mode)
{
    if (mode == 0)
        return c;
    if (mode == 4)
        return vec3(luma(c));
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
    vec3 err = c - lms2rgb * sim;
    return clamp(c + vec3(0.0, err.r * 0.7 + err.g, err.r * 0.7 + err.b), 0.0, 1.0);
}

void main()
{
    int filter_id = int(p.params.x + 0.5);
    float amount = clamp(p.params.y, 0.0, 1.0);
    vec2 uv = v_uv;
    vec3 col;
    float edge = 1.0;
    if (filter_id == 2)
        col = sharpen(uv, amount);
    else if (filter_id == 3)
        col = crt(uv, amount, 480.0, 0.35);
    else if (filter_id == 4)
    {
        /* The arcade monitor: the tube's glass bulges, the corners round off. */
        vec2 c = uv * 2.0 - 1.0;
        float k = mix(0.03, 0.10, amount);
        c *= 1.0 + k * dot(c, c) * vec2(0.75, 1.0);
        vec2 bent = c * 0.5 + 0.5;
        vec2 corner = max(abs(c) - (1.0 - 0.06), 0.0);
        edge = 1.0 - smoothstep(0.0, 0.012, length(corner) - 0.045);
        edge *= step(abs(c.x), 1.0) * step(abs(c.y), 1.0);
        col = crt(bent, mix(0.6, 1.0, amount), 240.0, 0.6);
        col *= 1.0 - 0.35 * dot(c * 0.7, c * 0.7);
    }
    else if (filter_id == 5)
        col = vhs(uv, amount);
    else if (filter_id == 6)
        col = soft_vhs(uv, amount);
    else if (filter_id == 7)
        col = eight_bit(uv, amount);
    else if (filter_id == 8)
        col = pocket(uv, amount);
    else if (filter_id == 9)
        col = scanlines(uv, amount);
    else if (filter_id == 10)
        col = shadow_mask(uv, amount);
    else if (filter_id == 11)
        col = lcd(uv, amount);
    else if (filter_id == 12)
    {
        /* Only when the picture is scaled up; then sharpened lightly by the
         * filter strength, as FSR's own sharpening pass does. */
        if (p.size.z > p.size.x * 1.01 || p.size.w > p.size.y * 1.01)
        {
            col = fsr_easu(uv);
            if (amount > 0.0)
            {
                vec3 plain = sample_rgb(uv);
                vec3 sharp = sharpen(uv, amount);
                col = clamp(col + (sharp - plain) * amount, 0.0, 1.0);
            }
        }
        else
            col = sharpen(uv, amount);
    }
    else
        col = texture(tex, uv).rgb;
    col = colour_filter(col, int(p.params.w + 0.5));
    frag = vec4(col * edge, 1.0) * p.color;
}
