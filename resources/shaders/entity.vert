#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec4 aJoints;
layout (location = 4) in vec4 aWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uBones[64];

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

mat4 skin_matrix() {
    float wsum = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    if (wsum <= 0.000001)
        return mat4(1.0);

    vec4 w = aWeights / wsum;
    int j0 = clamp(int(aJoints.x), 0, 63);
    int j1 = clamp(int(aJoints.y), 0, 63);
    int j2 = clamp(int(aJoints.z), 0, 63);
    int j3 = clamp(int(aJoints.w), 0, 63);
    return uBones[j0] * w.x
        +  uBones[j1] * w.y
        +  uBones[j2] * w.z
        +  uBones[j3] * w.w;
}

void main() {
    mat4 skin = skin_matrix();
    vec4 skinnedPos = skin * vec4(aPos, 1.0);
    vec3 skinnedNrm = normalize(mat3(skin) * aNormal);

    vec4 worldPos = uModel * skinnedPos;
    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(uModel))) * skinnedNrm;
    TexCoord = aUV;
    gl_Position = uProj * uView * worldPos;
}

