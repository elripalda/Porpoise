#version 450
/* Porpoise: the game's picture on its way to the TV, with an optional screen
 * filter. Copyright (C) 2026 Ruben (Project Porpoise), GPL-3.0-or-later.
 *
 * params: x filter (0 smooth, 1 sharp, 2 sharpen, 3 CRT, 4 arcade CRT, 5 VHS),
 *         y strength 0..1, z time in seconds, w unused.
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
    else
        col = texture(tex, uv).rgb;
    frag = vec4(col * edge, 1.0) * p.color;
}
