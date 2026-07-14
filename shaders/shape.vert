#version 450
layout(location = 0) in vec2 inPos;   // screen-space pixels
layout(location = 1) in float inGray;
layout(location = 2) in float inAlpha;
layout(location = 3) in vec2 inCapA;   // SDF param A (see primitives.h Vertex comment)
layout(location = 4) in vec2 inCapB;   // SDF param B
layout(location = 5) in float inRadius; // SDF radius
layout(location = 6) in float inKind;   // 0 = flat, 1 = capsule, 2 = box

layout(push_constant) uniform PushConstants { vec2 screenSize; } pc;

layout(location = 0) out float outGray;
layout(location = 1) out float outAlpha;
layout(location = 2) out vec2 outFragPos;
layout(location = 3) out vec2 outCapA;
layout(location = 4) out vec2 outCapB;
layout(location = 5) out float outRadius;
layout(location = 6) out float outKind;

void main() {
    vec2 ndc = (inPos / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    outGray = inGray;
    outAlpha = inAlpha;
    outFragPos = inPos;
    outCapA = inCapA;
    outCapB = inCapB;
    outRadius = inRadius;
    outKind = inKind;
}
