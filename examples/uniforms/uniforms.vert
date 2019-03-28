#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColour;
layout(location = 0) out vec3 fragColour;

// Instead of push_constant we use a binding.
layout (binding = 0) uniform Uniform {
  vec4 colour;
  mat4 rotation;
} u;

void main() {
  // Rotate and copy 2D position to 3D + depth
  gl_Position = u.rotation * vec4(inPosition, 0.0, 1.0);

  // Copy colour to the fragment shader.
  fragColour = inColour;
}
