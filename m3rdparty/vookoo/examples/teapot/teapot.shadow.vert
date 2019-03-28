#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout (binding = 0) uniform Uniform {
  mat4 modelToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  mat4 modelToLight;
  vec4 cameraPos;
} u;

out gl_PerVertex {
  vec4 gl_Position;
};

void main() {
  gl_Position = u.modelToLight * vec4(inPosition.xyz, 1.0);
}

