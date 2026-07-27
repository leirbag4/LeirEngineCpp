#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    vec3 lightDir;
    vec3 lightColor;
    vec3 ambientColor;
    vec4 color;
} push;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(push.lightDir);

    float diff = max(dot(normal, -lightDir), 0.0);
    vec3 diffuse = push.lightColor * diff;

    vec3 ambient = push.ambientColor;

    vec4 texColor = texture(texSampler, fragTexCoord);
    vec4 baseColor = texColor * push.color;

    outColor = vec4((ambient + diffuse) * baseColor.rgb, baseColor.a);
}
