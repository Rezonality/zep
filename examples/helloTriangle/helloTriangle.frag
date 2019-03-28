#version 450

layout(location = 0) in vec3 fragColour;
layout(location = 0) out vec4 outColour;

void main() {
  // Copy interpolated colour to the screen.
  outColour = vec4(fragColour, 1);
}
