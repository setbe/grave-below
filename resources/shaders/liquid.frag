#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FaceUvRect;
in float Ao;
in vec3 LocalPos;
flat in uint BlockId;

out vec4 FragColor;

uniform sampler2D uAtlas;
uniform vec2 uAtlasTexel;
uniform float uViewPosX;
uniform float uViewPosY;
uniform float uViewPosZ;
uniform float uSunDirX;
uniform float uSunDirY;
uniform float uSunDirZ;
uniform float uDaylight;
uniform float uFogR;
uniform float uFogG;
uniform float uFogB;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uBaseAlpha;
uniform float uFresnelPower;
uniform float uFresnelStrength;
uniform float uEdgeSoftness;
uniform float uEdgeStrength;
uniform int uOitPass; // 0: accum, 1: reveal

bool is_liquid(uint id) {
    return id == 5u || id == 13u || id == 6u || id == 14u || id == 7u || id == 15u || id == 20u || id == 21u;
}

uint liquid_profile(uint id) {
    if (id == 5u || id == 13u) return 0u; // water
    if (id == 6u || id == 14u) return 1u; // blood
    if (id == 7u || id == 15u) return 2u; // slime
    return 0u;
}

vec3 profile_tint(uint p) {
    if (p == 0u) return vec3(0.20, 0.36, 0.56);
    if (p == 1u) return vec3(0.58, 0.12, 0.16);
    if (p == 2u) return vec3(0.20, 0.52, 0.24);
    return vec3(0.20, 0.36, 0.56);
}

vec3 profile_fresnel(uint p) {
    if (p == 0u) return vec3(0.20, 0.36, 0.56);
    if (p == 1u) return vec3(0.42, 0.08, 0.10);
    if (p == 2u) return vec3(0.14, 0.38, 0.18);
    return vec3(0.20, 0.36, 0.56);
}

float profile_alpha(uint p) {
    if (p == 0u) return 1.00;
    if (p == 1u) return 1.08;
    if (p == 2u) return 0.96;
    return 1.00;
}

void main() {
    if (!is_liquid(BlockId))
        discard;
    // Enforce one-sided liquid rendering for all liquid profiles.
    // This guarantees back faces are culled uniformly even if pipeline state
    // is altered by another pass.
    if (!gl_FrontFacing)
        discard;

    const float EPS = 1e-5;
    vec3 n = normalize(Normal);
    vec3 viewPos = vec3(uViewPosX, uViewPosY, uViewPosZ);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 sunDir = normalize(vec3(uSunDirX, uSunDirY, uSunDirZ));
    float daylight = clamp(uDaylight, 0.0, 1.0);

    uint profile = liquid_profile(BlockId);
    // For layered liquids we rely on mesh-provided TexCoord so side faces
    // can be clipped against neighbor liquid height without UV stretching.
    vec2 base_uv = clamp(TexCoord, vec2(0.0), vec2(1.0));
    vec2 tiled = base_uv;
    vec2 inset = uAtlasTexel;
    vec2 tile_span = FaceUvRect.zw - FaceUvRect.xy;
    vec2 span_clamped = max(tile_span, vec2(EPS));
    vec2 inner_scale = max(vec2(0.0), vec2(1.0) - inset * 2.0);
    tiled = inset + tiled * inner_scale;
    vec2 uv = FaceUvRect.xy + tiled * span_clamped;
    vec2 grad_x = dFdx(tiled) * span_clamped;
    vec2 grad_y = dFdy(tiled) * span_clamped;
    vec3 tex = textureGrad(uAtlas, uv, grad_x, grad_y).rgb;

    float shimmer = 0.92 + 0.08 * sin(FragPos.x * 0.23 + FragPos.z * 0.19 + FragPos.y * 0.11);
    vec3 tint = profile_tint(profile);
    vec3 base_color = mix(tint, tex * tint, 0.32) * shimmer;

    float ndl = max(dot(n, sunDir), 0.0);
    float ao = (Ao > 0.001) ? Ao : 1.0;
    float ambient = mix(0.06, 0.20, daylight);
    float diffuse = mix(0.08, 0.42, daylight);
    vec3 lit = base_color * (ambient + ndl * diffuse) * ao;

    float fres = pow(1.0 - max(dot(n, viewDir), 0.0), max(0.001, uFresnelPower));
    lit += profile_fresnel(profile) * (fres * max(0.0, uFresnelStrength));

    float edge = 0.0;
    if (uEdgeStrength > 0.001) {
        vec3 cell = fract(LocalPos + vec3(0.0001));
        vec3 m = min(cell, vec3(1.0) - cell);
        float edge_dist = min(m.x, min(m.y, m.z));
        float soft = max(0.0001, uEdgeSoftness);
        edge = 1.0 - smoothstep(0.0, soft, edge_dist);
    }
    lit += profile_fresnel(profile) * edge * uEdgeStrength * 0.25;

    float dist = length(viewPos - FragPos);
    float fogStart = max(0.0, uFogStart);
    float fogEnd = max(fogStart + 1.0, uFogEnd);
    float fog = smoothstep(fogStart, fogEnd, dist);
    vec3 fogColor = vec3(uFogR, uFogG, uFogB);
    vec3 result = mix(lit, fogColor, fog);

    float alpha = clamp(uBaseAlpha * profile_alpha(profile), 0.05, 0.92);

    // Distance-driven liquid opacity bands (constants by design):
    // near <= 16 blocks: keep mostly transparent
    // 32..64 blocks: linearly become denser
    // >= 64 blocks: near-opaque
    const float kNearClearDist = 16.0;
    const float kFarFadeStartDist = 32.0;
    const float kFarFadeEndDist = 64.0;

    float near_t = clamp(dist / max(1e-4, kNearClearDist), 0.0, 1.0);
    float far_t = clamp((dist - kFarFadeStartDist) / max(1e-4, (kFarFadeEndDist - kFarFadeStartDist)), 0.0, 1.0);

    // Fresnel at grazing angles; stronger in far range.
    float angle_opacity = fres * mix(0.30, 1.00, far_t);
    alpha *= mix(0.88, 1.00, near_t);
    alpha = clamp(alpha * (1.0 + angle_opacity * 0.55), 0.05, 0.97);
    alpha = mix(alpha, 0.97, far_t);
    if (uOitPass == 1) {
        FragColor = vec4(alpha, alpha, alpha, alpha);
        return;
    }

    if (uOitPass < 0) {
        FragColor = vec4(result, alpha);
        return;
    }

    float weight = clamp(0.10 + alpha * 4.0, 0.10, 4.0);
    FragColor = vec4(result * alpha * weight, alpha * weight);
}
