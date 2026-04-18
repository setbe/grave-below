#version 330 core
out vec4 FragColor;
uniform float uAlpha;

void main()
{
    FragColor = vec4(0.0f, 0.0f, 0.0f, uAlpha);
} 
