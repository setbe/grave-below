#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FaceUvRect;
in float Ao;
in vec3 LocalPos;
flat in uint BlockId;

out vec4 FragColor;

uniform float uLightPosX;
uniform float uLightPosY;
uniform float uLightPosZ;

uniform float uViewPosX;
uniform float uViewPosY;
uniform float uViewPosZ;
uniform sampler2D uAtlas;
uniform int uChunkTiledMode;
uniform vec2 uAtlasTexel;
uniform float uSunDirX;
uniform float uSunDirY;
uniform float uSunDirZ;
uniform float uDaylight;
uniform float uFogR;
uniform float uFogG;
uniform float uFogB;
uniform float uFogStart;
uniform float uFogEnd;

bool is_liquid_id(uint id) {
    return id == 5u || id == 6u || id == 7u || id == 13u || id == 14u || id == 15u;
}

vec2 chunk_local_base_uv(vec3 local_pos, vec3 nrm) {
    vec3 an = abs(nrm);
    if (an.x > an.y && an.x > an.z) {
        // X faces: U follows Z, V follows Y.
        return (nrm.x > 0.0) ? vec2(-local_pos.z, local_pos.y) : vec2(local_pos.z, local_pos.y);
    } else if (an.y > an.z) {
        // Y faces: U follows X, V follows Z.
        return (nrm.y > 0.0) ? vec2(local_pos.x, local_pos.z) : vec2(local_pos.x, -local_pos.z);
    }
    // Z faces: U follows X, V follows Y.
    return (nrm.z > 0.0) ? vec2(local_pos.x, local_pos.y) : vec2(-local_pos.x, local_pos.y);
}

void main() {
    if (is_liquid_id(BlockId))
        discard;

    vec3 lightPos = vec3(uLightPosX, uLightPosY, uLightPosZ);
    vec3 viewPos = vec3(uViewPosX, uViewPosY, uViewPosZ);
    vec3 n = normalize(Normal);
    float daylight = clamp(uDaylight, 0.0, 1.0);

    vec2 uv = TexCoord;
    vec4 texel;
    if (uChunkTiledMode != 0) {
        vec2 base_uv = chunk_local_base_uv(LocalPos, n);
        vec2 tiled = fract(base_uv + vec2(0.0001));
        // keep sampling inside tile borders to reduce atlas bleeding.
        vec2 inset = uAtlasTexel;
        vec2 tile_span = FaceUvRect.zw - FaceUvRect.xy;
        vec2 span_clamped = max(tile_span, vec2(0.000001));
        vec2 inner_scale = max(vec2(0.0), vec2(1.0) - inset * 2.0);
        tiled = inset + tiled * inner_scale;
        uv = FaceUvRect.xy + tiled * span_clamped;

        // Preserve proper mip selection for wrapped UV on large greedy quads.
        vec2 grad_x = dFdx(base_uv) * span_clamped * inner_scale;
        vec2 grad_y = dFdy(base_uv) * span_clamped * inner_scale;
        texel = textureGrad(uAtlas, uv, grad_x, grad_y);
    } else
        texel = texture(uAtlas, uv);

    if (texel.a < 0.10)
        discard;

    // Stable sun direction (camera-independent) from renderer time-of-day state.
    vec3 sunDir = normalize(vec3(uSunDirX, uSunDirY, uSunDirZ));
    if (length(sunDir) < 0.0001)
        sunDir = normalize(lightPos - FragPos);
    float ndl = max(dot(n, sunDir), 0.0);

    // Minecraft-like face shading.
    float faceShade = 0.78;
    if (n.y > 0.8) faceShade = 1.00;
    else if (n.y < -0.8) faceShade = 0.76;
    else if (abs(n.z) > 0.8) faceShade = 0.86;

    float ambient = mix(0.08, 0.26, daylight);
    float diffuse = mix(0.20, 0.78, daylight);
    float light = ambient + ndl * diffuse;
    float stepped = floor(light * 16.0) / 16.0;
    light = mix(light, stepped, 0.35);
    float ao = Ao > 0.001 ? Ao : 1.0;
    vec3 lit = texel.rgb * (light * faceShade * ao);

    // Mild distance fog for chunk horizon.
    float dist = length(viewPos - FragPos);
    float fogStart = max(0.0, uFogStart);
    float fogEnd = max(fogStart + 1.0, uFogEnd);
    float fog = smoothstep(fogStart, fogEnd, dist);
    vec3 fogColor = vec3(uFogR, uFogG, uFogB);
    vec3 result = mix(lit, fogColor, fog);
    FragColor = vec4(result, texel.a);
}
