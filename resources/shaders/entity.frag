#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform float uLightPosX;
uniform float uLightPosY;
uniform float uLightPosZ;
uniform float uViewPosX;
uniform float uViewPosY;
uniform float uViewPosZ;
uniform sampler2D uAtlas;
uniform float uDaylight;
uniform float uFogR;
uniform float uFogG;
uniform float uFogB;

void main() {
    vec3 lightPos = vec3(uLightPosX, uLightPosY, uLightPosZ);
    vec3 viewPos = vec3(uViewPosX, uViewPosY, uViewPosZ);
    vec3 nrm = normalize(Normal);
    float daylight = clamp(uDaylight, 0.0, 1.0);
    vec4 tex_rgba = texture(uAtlas, TexCoord);
    if (tex_rgba.a < 0.05)
        discard;
    vec3 tex = tex_rgba.rgb;

    vec3 light_dir = normalize(lightPos - FragPos);
    float diff = max(dot(nrm, light_dir), 0.0);
    vec3 ambient = tex * mix(0.10, 0.32, daylight);
    vec3 diffuse = tex * (mix(0.22, 0.68, daylight) * diff);

    float dist = length(viewPos - FragPos);
    float fog_start = mix(45.0, 85.0, daylight);
    float fog_range = mix(95.0, 160.0, daylight);
    float fog = clamp((dist - fog_start) / fog_range, 0.0, 1.0);
    vec3 fog_color = vec3(uFogR, uFogG, uFogB);
    vec3 color = mix(ambient + diffuse, fog_color, fog);
    FragColor = vec4(color, 1.0);
}
