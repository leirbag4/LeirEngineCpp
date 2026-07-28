#version 450
layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 color;
    vec4 uvRect;
} push;

void main() {
    vec2 uv = push.uvRect.xy + fragTexCoord * push.uvRect.zw;
    outColor = texture(texSampler, uv) * push.color;
}
