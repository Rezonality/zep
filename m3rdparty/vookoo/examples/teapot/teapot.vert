#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUv;
layout(location = 2) out vec3 outCameraDir;
layout(location = 3) out vec4 outLightSpacePos;
layout(location = 4) out vec3 outLightDir;

layout (binding = 0) uniform Uniform {
  mat4 modelToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  mat4 modelToLight;
  vec4 cameraPos;
  vec4 lightPos;
} u;

out gl_PerVertex {
  vec4 gl_Position;
};

void main() {
  // Get position in camera, light and world space.
  gl_Position = u.modelToPerspective * vec4(inPosition.xyz, 1.0);
  vec4 lightSpacePos = u.modelToLight * vec4(inPosition.xyz, 1.0);
  vec3 worldPos = (u.modelToWorld * vec4(inPosition.xyz, 1.0)).xyz;

  // Centre at (0.5, 0.5) and scale to (0..1)
  outLightSpacePos = vec4(( lightSpacePos.xy + lightSpacePos.ww ) * 0.5, lightSpacePos.zw);

  //outLightSpacePos = u.modelToPerspective * vec4(inPosition.xyz, 1.0);
  outNormal = (u.normalToWorld * vec4(inNormal, 0.0)).xyz;
  outUv = inUv;
  outCameraDir = normalize(u.cameraPos.xyz - worldPos);
  outLightDir = normalize(u.lightPos.xyz - worldPos);
}

