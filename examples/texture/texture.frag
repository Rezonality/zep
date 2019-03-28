#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout (binding = 0) uniform UBO 
{
  vec4 colour;
} ubo;

layout (binding = 1) uniform sampler2D samp; 

void main() {
  outColor = texture(samp, fragColor.xy * 8.0f);
}

