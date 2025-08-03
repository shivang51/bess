#version 460 core

layout (location = 0) in vec2 a_LocalPosition; 
layout (location = 1) in vec2 a_LocalTexCoord;

layout(location = 2) in vec3 a_Position;
layout(location = 3) in vec2 a_Size;
layout(location = 4) in float a_Angle;
layout(location = 5) in vec4 a_Color;
layout(location = 6) in int a_FragId;
layout(location = 7) in int a_TexSlotIdx;

out vec3 v_FragPos;
out vec4 v_FragColor;
out vec2 v_TexCoord;
out flat int v_FragId;
out flat int v_TexSlotIdx;

uniform mat4 u_mvp;

void main() {
    vec2 transformedPos;
    float cosAngle = cos(a_Angle);
    float sinAngle = sin(a_Angle);
    mat2 rotationMatrix = mat2(
        cosAngle, -sinAngle,
        sinAngle,  cosAngle
    );

    transformedPos = rotationMatrix * (a_LocalPosition * a_Size);
    vec3 worldPos = a_Position + vec3(transformedPos, 0.0);

    gl_Position = u_mvp * vec4(worldPos, 1.0);

    v_FragColor = a_Color;
    v_TexCoord = a_LocalTexCoord;
    v_FragId = a_FragId;
    v_TexSlotIdx = a_TexSlotIdx;
}
