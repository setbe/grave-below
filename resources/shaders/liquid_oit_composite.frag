#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uAccum;
uniform sampler2D uReveal;
uniform sampler2D uDepth;
uniform float uSingleAlpha;
uniform float uNearPlane;
uniform float uFarPlane;

float linearize_depth(float depth01, float near_plane, float far_plane) {
    float z = depth01 * 2.0 - 1.0;
    return (2.0 * near_plane * far_plane) /
           max(1e-6, (far_plane + near_plane - z * (far_plane - near_plane)));
}

void main() {
    vec3 scene = texture(uScene, TexCoord).rgb;
    vec4 accum = texture(uAccum, TexCoord);
    float reveal = clamp(texture(uReveal, TexCoord).r, 0.0, 1.0);
    float depth01 = texture(uDepth, TexCoord).r;

    float weight = max(accum.a, 1e-5);
    vec3 liquid = accum.rgb / weight;
    float has_liquid = step(1e-5, accum.a);

    // Base alpha from reveal keeps layered liquids from becoming fully opaque
    // due to overlap, while still reacting to per-fragment alpha from the pass.
    float alpha_raw = clamp(1.0 - reveal, 0.0, 1.0) * has_liquid;
    float alpha = min(alpha_raw, clamp(uSingleAlpha, 0.0, 1.0));

    float view_dist = linearize_depth(depth01, max(0.01, uNearPlane), max(uNearPlane + 0.01, uFarPlane));

    vec3 out_rgb = mix(scene, liquid, alpha);
    FragColor = vec4(out_rgb, 1.0);
}
