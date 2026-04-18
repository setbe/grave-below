#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform float uBlackStrength;
uniform float uRedStrength;
uniform float uDeadStrength;
uniform vec2 uMapTintRg;
uniform float uMapTintB;
uniform float uMapTintStrength;
uniform float uRegionDecay;
uniform float uRegionInstability;

void main() {
    vec2 uv = TexCoord * 2.0 - 1.0;
    float radius = length(uv);

    float vignette = smoothstep(0.58, 1.02, radius);
    float red_edge = smoothstep(0.68, 1.05, radius);

    float black_alpha = clamp(vignette * uBlackStrength, 0.0, 1.0);
    float red_alpha = clamp(red_edge * uRedStrength, 0.0, 1.0);

    vec3 color = vec3(0.0);
    float alpha = black_alpha;

    if (red_alpha > 0.0) {
        vec3 red = vec3(0.65, 0.02, 0.02);
        color = mix(color, red, red_alpha);
        alpha = max(alpha, red_alpha);
    }

    if (uDeadStrength > 0.0) {
        color = vec3(0.0);
        alpha = max(alpha, clamp(uDeadStrength, 0.0, 1.0));
    }

    if (uMapTintStrength > 0.0) {
        float tint_alpha = clamp(uMapTintStrength, 0.0, 1.0);
        vec3 map_tint = vec3(uMapTintRg.x, uMapTintRg.y, uMapTintB);
        color = mix(color, map_tint, clamp(tint_alpha * 0.85, 0.0, 1.0));
        alpha = max(alpha, tint_alpha);
    }

    if (uRegionDecay > 0.0) {
        float fade = clamp(uRegionDecay * 0.30, 0.0, 0.30);
        vec3 dead_tint = vec3(0.58, 0.60, 0.63);
        color = mix(color, dead_tint, fade);
        alpha = max(alpha, fade * 0.8);
    }

    if (uRegionInstability > 0.0) {
        float n = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
        float pulse = (n - 0.5) * clamp(uRegionInstability * 0.06, 0.0, 0.06);
        color += vec3(pulse * 0.6, pulse * 0.3, pulse * 0.8);
    }

    FragColor = vec4(color, alpha);
}
