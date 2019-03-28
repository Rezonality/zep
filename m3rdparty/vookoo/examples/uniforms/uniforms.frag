#version 450

layout(location = 0) in vec3 fragColour;
layout(location = 0) out vec4 outColour;

// Instead of push_constant we use a binding.
layout (binding = 0) uniform Uniform {
  vec4 colour;
  mat4 rotation;
} u;

void main() {
  // Copy interpolated colour to the screen.
  outColour = vec4(fragColour, 1) * u.colour;
}
