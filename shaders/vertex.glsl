#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
layout(location=3) in vec2 aUV;
out vec2 vUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;

void main()
{
    vUV = aUV;

    vec4 worldPos = model * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = aNormal;      // still in model space; fragment uses normalMatrix
    vColor = aColor;

    gl_Position = projection * view * worldPos;
}
