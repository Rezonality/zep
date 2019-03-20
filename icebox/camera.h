#pragma once

#include <glm/glm.hpp>

#include "entities/entities.h"

namespace Mgfx
{

// Perspetive/ortho
enum class CameraMode : uint32_t
{
    Perspective = 0,
    Ortho = 1
};

// D3D or GL projection
// -Z into the screen, RHS currently
enum class ProjectionType : uint32_t
{
    GL,
    D3D
};

// A simple ray for ray tracing usage
struct Ray
{
    glm::vec3 position;
    glm::vec3 direction;
};

struct CameraComponent : Component
{
    COMPONENT(CameraComponent);

    CameraMode mode = CameraMode::Perspective;
    glm::vec3 focalPoint = glm::vec3(0.0f); // Look at point
    glm::uvec2 filmSize = glm::uvec2(0, 0); // Size of the buffer
    glm::vec2 depthRange = glm::vec2(0.1f, 100.0f); // Depth range for the projection

    glm::vec3 viewDirection = glm::vec3(0.0f, 0.0f, 1.0f); // The direction the camera is looking in
    glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f); // The vector to the right
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // The vector up

    float fieldOfView = 60.0f; // Field of view
    float halfAngle = 30.0f; // Half angle of the view frustum
    float aspectRatio = 1.0f; // Ratio of x to y of the viewport

    // Used to calculate camera updates based on inputs or animation
    glm::vec2 orbitDelta = glm::vec2(0.0f);
    glm::vec3 positionDelta = glm::vec3(0.0f);
    glm::vec3 walkDelta = glm::vec3(0.0f);

    virtual bool ShowUI() override;

    // Pixel size of the rendering
    void SetFilmSize(const glm::uvec2& size);

    // Does the camera still have some velocity?
    bool Moving();

    // Standard manipulation functions
    void Walk(const glm::vec3& planes);
    void Dolly(float distance);
    void Orbit(const glm::vec2& angle);
};

glm::mat4 camera_get_lookat(Entity* camera);
void camera_set_lookat(Entity* camera, const glm::vec3& pos, const glm::vec3& point);
glm::mat4 camera_get_projection(Entity* camera, ProjectionType type);
Ray camera_get_world_ray(Entity* camera, const glm::vec2& imageSample);

// ** All Components
void camera_components_update(TimeDelta dt);

} // namespace Mgfx
