#version 330 core
out vec4 FragColor;

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vUV;

uniform sampler2D uTex;
uniform int uUseTexture;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 lightColor;

uniform float ambientStrength;
uniform float specStrength;
uniform float shininess;

uniform float constantAtt;
uniform float linearAtt;
uniform float quadraticAtt;
uniform int uSelected;

uniform int useBlinnPhong;
uniform mat3 normalMatrix;
void main()
{
    // --- lighting vectors ---
    vec3 N = normalize(normalMatrix * vNormal);

    vec3 lightVec = lightPos - vWorldPos;
    float dist = length(lightVec);
    vec3 L = lightVec / max(dist, 0.0001);

    vec3 V = normalize(viewPos - vWorldPos);

    // --- attenuation ---
    float att = 1.0 / (constantAtt + linearAtt * dist + quadraticAtt * dist * dist);

    // --- ambient ---
    vec3 ambient = ambientStrength * lightColor;

    // --- diffuse ---
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * lightColor;

    // --- specular (Phong vs Blinn) ---
    float spec = 0.0;
    if (diff > 0.0)
    {
        if (useBlinnPhong == 1)
        {
            vec3 H = normalize(L + V);
            spec = pow(max(dot(N, H), 0.0), shininess);
        }
        else
        {
            vec3 R = reflect(-L, N);
            spec = pow(max(dot(V, R), 0.0), shininess);
        }
    }
    vec3 specular = specStrength * spec * lightColor;

    // --- final lighting ---
    vec3 lit = ambient + (diffuse + specular) * att;

    // --- base color (texture OR highlight) ---
    vec3 baseColor = vColor;

    if (uSelected == 1) {
        baseColor = vec3(1.0, 1.0, 0.2);   // bright yellow highlight
    } else if (uUseTexture == 1) {
        baseColor = texture(uTex, vUV).rgb;
    }

    vec3 finalColor = baseColor * lit;
    FragColor = vec4(finalColor, 1.0);
}