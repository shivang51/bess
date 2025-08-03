#version 460 core

layout (location = 0) in vec2 a_LocalPosition; 
layout (location = 1) in vec2 a_LocalTexCoord;

layout (location = 2) in vec3 a_InstancePosition;    
layout (location = 3) in vec4 a_InstanceColor;
layout (location = 4) in vec4 a_InstanceBorderRadius;
layout (location = 5) in vec4 a_InstanceBorderColor;
layout (location = 6) in vec4 a_InstanceBorderSize;
layout (location = 7) in vec2 a_InstanceSize;
layout (location = 8) in int  a_InstanceId;
layout (location = 9) in int a_InstanceIsMica;
layout (location = 10) in int a_InstanceTexSlotIdx;
// To enable rotation, add 'float angle' to your C++ QuadVertex struct
// and uncomment the following line and the rotation logic in main().
// layout (location = 11) in float a_InstanceAngle;


out vec4 v_FragColor;
out vec2 v_TexCoord;
out vec4 v_BorderRadius;
out vec4 v_BorderSize;
out vec4 v_BorderColor;
out vec2 v_Size;
out flat int v_FragId;
out int v_IsMica;
out flat int v_TexSlotIdx;

uniform mat4 u_mvp;

void main() {
    vec2 transformedPos;

    /*
    // --- Optional Rotation Logic ---
    // 1. Create a 2D rotation matrix from the instance's angle
    float cosAngle = cos(a_InstanceAngle);
    float sinAngle = sin(a_InstanceAngle);
    mat2 rotationMatrix = mat2(
        cosAngle, -sinAngle,
        sinAngle,  cosAngle
    );

    // 2. Scale the local corner position by the instance's size, then rotate it
    transformedPos = rotationMatrix * (a_LocalPosition * a_InstanceSize);
    */

    // 2. Scale the local corner position by the instance's size
    transformedPos = a_LocalPosition * a_InstanceSize;

    // 3. Translate the transformed corner to the instance's center position
    vec3 worldPos = a_InstancePosition + vec3(transformedPos, 0.0);

    // 4. Apply the camera's view-projection matrix
    gl_Position = u_mvp * vec4(worldPos, 1.0);


    // --- Pass all other attributes to the Fragment Shader ---
    // These are passed through directly without modification.
    v_FragColor = a_InstanceColor;
    v_TexCoord = a_LocalTexCoord; // Pass the per-vertex texture coordinate
    v_BorderRadius = a_InstanceBorderRadius;
    v_BorderColor = a_InstanceBorderColor;
    v_BorderSize = a_InstanceBorderSize;
    v_FragId = a_InstanceId;
    v_Size = a_InstanceSize;
    v_IsMica = a_InstanceIsMica;
    v_TexSlotIdx = a_InstanceTexSlotIdx;
}