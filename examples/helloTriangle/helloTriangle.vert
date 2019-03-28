#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColour;
layout(location = 0) out vec3 fragColour;

void main() {
  gl_Position = vec4(inPosition, 0.0, 1.0); // Copy 2D position to 3D + depth
  fragColour = inColour;                    // Copy colour to the fragment shader.
}
