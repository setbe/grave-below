#version 330 core
in vec2 vNdc;
out vec4 FragColor;

uniform vec2 uScreen;
uniform float uCamForwardX;
uniform float uCamForwardY;
uniform float uCamForwardZ;
uniform float uCamRightX;
uniform float uCamRightY;
uniform float uCamRightZ;
uniform float uCamUpX;
uniform float uCamUpY;
uniform float uCamUpZ;
uniform float uAspect;
uniform float uTanHalfFov;
uniform float uTimeSec;
uniform float uDayLengthSec;
uniform float uDaylight;
uniform float uEclipse;
uniform float uRegionMana;
uniform float uRegionInstability;
uniform float uRegionDecay;

const float PI = 3.14159265359;
const float TAU = 6.28318530718;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

float hash13(vec3 p3) {
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float disk_soft(vec3 rd, vec3 center, float radius, float softness) {
    vec3 cd = normalize(center);
    float dp = dot(rd, cd);
    float outer = cos(radius);
    float inner = cos(max(radius - softness, 0.0001));
    return smoothstep(outer, inner, dp);
}

vec3 rotate_axis(vec3 v, vec3 axis, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return v * c + cross(axis, v) * s + axis * dot(axis, v) * (1.0 - c);
}

void main() {
    vec3 cam_forward = normalize(vec3(uCamForwardX, uCamForwardY, uCamForwardZ));
    vec3 cam_right = normalize(vec3(uCamRightX, uCamRightY, uCamRightZ));
    vec3 cam_up = normalize(vec3(uCamUpX, uCamUpY, uCamUpZ));
    vec2 scr = max(uScreen, vec2(1.0));
    vec2 ndc = (gl_FragCoord.xy / scr) * 2.0 - 1.0;
    vec3 rd = normalize(cam_forward
                      + cam_right * (ndc.x * uAspect * uTanHalfFov)
                      + cam_up * (ndc.y * uTanHalfFov));

    float safe_day_len = max(uDayLengthSec, 1.0);
    float day_phase = fract(uTimeSec / safe_day_len);
    float sun_ang = day_phase * TAU;
    vec3 sun_dir = normalize(vec3(cos(sun_ang), sin(sun_ang), 0.28));
    float daylight = clamp(uDaylight, 0.0, 1.0);
    float eclipse = clamp(uEclipse, 0.0, 1.0);
    float region_mana = clamp(uRegionMana, 0.0, 1.0);
    float region_instability = clamp(uRegionInstability, 0.0, 1.0);
    float region_decay = clamp(uRegionDecay, 0.0, 1.0);

    float h = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 day_top = vec3(0.48, 0.68, 0.98);
    vec3 day_horizon = vec3(0.76, 0.86, 1.00);
    vec3 night_top = vec3(0.010, 0.015, 0.050);
    vec3 night_horizon = vec3(0.040, 0.050, 0.120);
    vec3 day_sky = mix(day_horizon, day_top, pow(h, 0.65));
    vec3 night_sky = mix(night_horizon, night_top, pow(h, 0.70));
    vec3 sky = mix(night_sky, day_sky, daylight);
    float decay_fade = region_decay * (0.26 + 0.24 * (1.0 - region_mana));
    vec3 sky_gray = vec3(dot(sky, vec3(0.299, 0.587, 0.114)));
    sky = mix(sky, sky_gray, clamp(decay_fade, 0.0, 0.72));

    // Seam-free clouds (avoid zenith/pole artifacts from spherical UV seams).
    float cloud_h = max(rd.y + 0.34, 0.14);
    vec2 cloud_uv = rd.xz / cloud_h + vec2(region_instability * 0.43, -region_instability * 0.37);
    cloud_uv += vec2(uTimeSec * 0.0025, uTimeSec * 0.0014);
    float cloud0 = noise2(cloud_uv * vec2(1.8, 1.6));
    float cloud1 = noise2(cloud_uv * vec2(3.9, 3.4) - vec2(uTimeSec * 0.0007, 0.0));
    float cloud_mix = cloud0 * 0.68 + cloud1 * 0.32;
    float clouds = smoothstep(0.57, 0.76, cloud_mix);
    float cloud_mask = clouds * smoothstep(-0.02, 0.42, rd.y) * daylight;
    sky = mix(sky, vec3(0.97, 0.98, 1.00), cloud_mask * 0.58);

    // Stable star field: fixed seeds + smooth celestial rotation.
    vec3 star_axis = normalize(vec3(0.09, 0.995, -0.035));
    vec3 rd_star = rotate_axis(rd, star_axis, day_phase * TAU * 1.01 + 0.21);
    vec2 star_uv = vec2(
        atan(rd_star.z, rd_star.x) / TAU + 0.5,
        asin(clamp(rd_star.y, -1.0, 1.0)) / PI + 0.5
    );
    vec2 star_grid = vec2(620.0, 320.0);
    vec2 star_pos = star_uv * star_grid;
    vec2 star_base = floor(star_pos);
    vec2 star_frac = fract(star_pos);

    float star = 0.0;
    vec3 star_col = vec3(0.0);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 cell = star_base + vec2(float(x), float(y));
            cell.x = mod(cell.x + star_grid.x, star_grid.x);
            cell.y = clamp(cell.y, 0.0, star_grid.y - 1.0);

            float spawn = hash12(cell + vec2(11.3, 91.7));
            if (spawn > 0.9962) {
                vec2 rnd = vec2(
                    hash12(cell + vec2(1.7, 2.9)),
                    hash12(cell + vec2(4.3, 8.8))
                );
                vec2 center = vec2(float(x), float(y)) + rnd;
                vec2 d = center - star_frac;
                float dist = length(d);
                float size = mix(0.048, 0.112, hash12(cell + vec2(3.2, 6.4)));
                float bright = mix(0.55, 1.24, hash12(cell + vec2(8.6, 5.1)));
                float twinkle = 0.92 + 0.08 * sin(uTimeSec * (0.70 + hash12(cell + vec2(12.1, 1.3)) * 1.6) + spawn * 61.0);
                float s = smoothstep(size, 0.0, dist) * bright * twinkle;

                vec3 c = vec3(1.0);
                float tint_seed = hash12(cell + vec2(21.0, 3.7));
                if (tint_seed > 0.9976) {
                    float mute = 0.75 + 0.15 * hash12(cell + vec2(9.9, 14.4));
                    float ch1 = hash12(cell + vec2(2.2, 17.7));
                    if (ch1 < 0.333) c.r *= mute;
                    else if (ch1 < 0.666) c.g *= mute;
                    else c.b *= mute;

                    if (hash12(cell + vec2(15.3, 7.1)) > 0.88) {
                        float ch2 = hash12(cell + vec2(6.7, 19.2));
                        if (ch2 < 0.333) c.r *= mute;
                        else if (ch2 < 0.666) c.g *= mute;
                        else c.b *= mute;
                    }
                }

                star += s;
                star_col += c * s;
            }
        }
    }
    float night_factor = 1.0 - smoothstep(0.17, 0.185, daylight);
    star *= night_factor * smoothstep(-0.10, 0.55, rd.y);
    star_col *= night_factor * smoothstep(-0.10, 0.55, rd.y);
    star *= mix(0.72, 1.18, clamp(region_instability + (1.0 - region_mana) * 0.35, 0.0, 1.0));
    if (star > 0.0001) star_col /= star;
    else star_col = vec3(1.0);

    // Sun disk.
    float sun_disk = disk_soft(rd, sun_dir, 0.055, 0.013);
    vec3 sun_col = vec3(1.00, 0.92, 0.62);

    // Planetary ring crossing the whole sky (dusty great-circle band on sky dome).
    vec3 ring_normal = rotate_axis(normalize(vec3(0.98, 0.08, 0.16)), vec3(0.0, 0.0, 1.0), 0.095);
    vec3 ring_up = abs(ring_normal.y) > 0.98 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 ring_t = normalize(cross(ring_up, ring_normal));
    vec3 ring_b = normalize(cross(ring_normal, ring_t));
    float ring_lat = abs(dot(rd, ring_normal));
    float ring_long = atan(dot(rd, ring_b), dot(rd, ring_t)) / TAU + 0.5;
    float thick0 = noise2(vec2(ring_long * 28.0 + 3.0, 11.0));
    float thick1 = noise2(vec2(ring_long * 83.0 + 17.0, 27.0));
    float ring_outer = 0.046 + (thick0 - 0.5) * 0.018 + (thick1 - 0.5) * 0.010;
    ring_outer = clamp(ring_outer, 0.028, 0.064);
    float ring_inner = ring_outer * (0.10 + 0.10 * noise2(vec2(ring_long * 51.0 + 9.0, 39.0)));
    float ring_band = smoothstep(ring_outer, ring_inner, ring_lat);

    float gap0 = noise2(vec2(ring_long * 22.0 + 7.0, 5.0));
    float gap1 = noise2(vec2(ring_long * 71.0 + 19.0, 13.0));
    float gap_mix = gap0 * 0.65 + gap1 * 0.35;
    float dark_gaps = 1.0 - smoothstep(0.78, 0.95, gap_mix) * 0.34;
    float rare_gaps = 1.0 - smoothstep(0.965, 0.995, noise2(vec2(ring_long * 9.0 + 31.0, 61.0))) * 0.45;
    ring_band *= dark_gaps * rare_gaps;

    // Hide lower hemisphere part and softly fade out near horizon.
    float horizon_fade = smoothstep(-0.10, 0.24, rd.y);
    ring_band *= horizon_fade;

    float edge_noise = noise2(vec2(ring_long * 610.0 + 5.0, ring_lat * 3800.0 + 13.0));
    float edge_outer = smoothstep(0.010 + edge_noise * 0.006, 0.0, abs(ring_lat - ring_outer));
    float edge_inner = smoothstep(0.012 + edge_noise * 0.005, 0.0, abs(ring_lat - ring_inner));
    float ring_edge = clamp(edge_outer + edge_inner, 0.0, 1.0);

    float dust0 = noise2(vec2(ring_long * 460.0 + 29.0, ring_lat * 2200.0 + 47.0));
    float dust1 = noise2(vec2(ring_long * 920.0 + 73.0, ring_lat * 4600.0 + 91.0));
    float dust_density = dust0 * 0.62 + dust1 * 0.38;
    float ring_grain = noise2(vec2(ring_long * 380.0 + 13.0, ring_lat * 1300.0 + 17.0));
    vec3 ring_col = mix(vec3(0.78, 0.74, 0.58), vec3(0.96, 0.90, 0.74), ring_grain);
    ring_col *= mix(0.72, 1.08, dust_density);
    ring_col = mix(ring_col, vec3(0.99, 0.95, 0.82), ring_edge * 0.42);

    // Day: dimmer than sun. Night: brightest stable object.
    float night_ring = 1.0 - smoothstep(0.22, 0.86, daylight);
    float ring_vis = mix(0.10, 1.06, night_ring);
    ring_vis *= mix(0.16, 1.04, region_mana);
    ring_vis *= mix(1.0, 0.55, region_decay);
    ring_band *= ring_vis;

    // Daily short solar dim (sun crosses ring around noon), plus major 7-day eclipse.
    float sun_lat = abs(dot(sun_dir, ring_normal));
    float ring_pass = smoothstep(0.040, 0.004, sun_lat);
    float noon_dist = abs(day_phase - 0.25);
    noon_dist = min(noon_dist, 1.0 - noon_dist);
    float noon_window = smoothstep(0.032, 0.0, noon_dist);
    float daily_occult = ring_pass * noon_window;
    float major_occult = eclipse;
    float sun_occult = clamp(daily_occult * 0.60 + major_occult * 0.88, 0.0, 0.96);
    sun_disk *= (1.0 - sun_occult);

    float sky_dark = clamp(major_occult * daylight * 0.58 + daily_occult * daylight * 0.18, 0.0, 0.75);
    sky = mix(sky, night_sky, sky_dark);

    float eclipse_cool = clamp(major_occult * daylight * 0.48 + daily_occult * daylight * 0.16, 0.0, 0.65);
    sky *= mix(vec3(1.0), vec3(0.90, 0.96, 1.06), eclipse_cool);
    float horizon = 1.0 - smoothstep(0.24, 0.80, h);
    sky *= 1.0 - eclipse_cool * horizon * 0.22;

    vec3 color = sky;
    float direct_dim = 1.0 - clamp(major_occult * daylight * 0.45 + daily_occult * daylight * 0.20, 0.0, 0.70);
    color += sun_col * (sun_disk * (0.95 + daylight * 0.55) * direct_dim);
    color += ring_col * ring_band;
    color += star_col * (star * 1.30);

    FragColor = vec4(color, 1.0);
}
