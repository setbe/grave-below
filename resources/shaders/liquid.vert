#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec4 aFaceUvRect;
layout (location = 4) in float aAo;
layout (location = 5) in uint aBlockId;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FaceUvRect;
out float Ao;
out vec3 LocalPos;
flat out uint BlockId;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    TexCoord = aUV;
    FaceUvRect = aFaceUvRect;
    Ao = aAo;
    LocalPos = aPos;
    BlockId = aBlockId;
    gl_Position = uProj * uView * worldPos;
}
