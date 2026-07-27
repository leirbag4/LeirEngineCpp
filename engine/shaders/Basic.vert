#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 viewProjection;
} ubo;

layout(push_constant) uniform PushConstants {
    vec3 lightDir;
    float pad0;
    vec3 lightColor;
    float pad1;
    vec3 ambientColor;
    float pad2;
    vec4 color;
    mat4 model;
} push;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.viewProjection * worldPos;
    fragNormal = mat3(push.model) * inNormal;
    fragTexCoord = inTexCoord;
    fragWorldPos = worldPos.xyz;
}
