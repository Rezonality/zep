#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inCameraDir;
layout(location = 3) in vec4 inLightSpacePos;
layout(location = 4) in vec3 inLightDir;

layout(location = 0) out vec4 outColour;

layout (binding = 0) uniform Uniform {
  mat4 modelToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  mat4 modelToLight;
  vec4 cameraPos;
  vec4 lightPos;
} u;

layout (binding = 1) uniform samplerCube cubeMap;
layout (binding = 2) uniform sampler2D shadowMap;

void main() {
  vec3 cameraDir = normalize(inCameraDir);
  vec3 normal = normalize(inNormal);
  vec3 lightDir = normalize(inLightDir);

  vec3 reflectDir = normalize(reflect(-cameraDir, normal));
  vec3 reflect = texture(cubeMap, reflectDir).xyz;

  vec3 ambient = vec3(0.3, 0.3, 0.1);
  vec3 diffuse = vec3(0.9, 0.9, 0.1);
  vec3 specular = vec3(0.3, 0.3, 0.3);

  float diffuseFactor = max(0.0, dot(normal, lightDir));
  float shadowDepth = textureProj(shadowMap, inLightSpacePos).x;
  float expectedDepth = inLightSpacePos.z / inLightSpacePos.w;
  float shadowFactor = expectedDepth < shadowDepth ? 1.0 : 0.0;
  outColour = vec4(ambient + diffuse * diffuseFactor * shadowFactor + specular * reflect, 1);
}

