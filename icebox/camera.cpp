#include "utils.h"

#include "glm/glm/gtc/random.hpp"
#include "glm/glm/gtc/matrix_transform.hpp"

#include "device/dx12/device_dx12.h"

#include <imgui/imgui.h>

#include "math/mathutils.h"


#include "animation/timer.h"

#include <glm/gtx/norm.hpp>
#include "camera.h"
#include "body.h"
#include "jorvik.h"

namespace Mgfx
{

void update_right_up(Entity* entity)
{
    auto camera = component_get<CameraComponent>(entity);
    auto& position = component_get<BodyComponent>(entity)->position;
    auto& orientation = component_get<BodyComponent>(entity)->orientation;

    // Right and up vectors updated based on the quaternion orientation
    camera->right = glm::normalize(orientation * glm::vec3(1.0f, 0.0f, 0.0f));
    camera->up = glm::normalize(orientation * glm::vec3(0.0f, 1.0f, 0.0f));
}

bool CameraComponent::ShowUI()
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::DragFloat3("Focal Point", &focalPoint[0]))
            changed = true;

        ImGui::LabelText("Film Size", "%f, %f", &filmSize.x, &filmSize.y);
        
        ImGui::DragFloat2("Depth Range", &depthRange[0]);

        if (ImGui::DragFloat3("View Direction", &viewDirection[0]))
            changed = true;
        if (ImGui::DragFloat3("Right", &right[0]))
            changed = true;
        if (ImGui::DragFloat3("Up", &up[0]))
            changed = true;

        if (ImGui::DragFloat("Field Of View", &fieldOfView))
            changed = true;
        if (ImGui::DragFloat("Half Angle", &halfAngle))
            changed = true;
        if (ImGui::DragFloat("Aspect Ratio", &aspectRatio))
            changed = true;

        if (ImGui::DragFloat2("Orbit Delta", &orbitDelta[0]))
            changed = true;
        if (ImGui::DragFloat2("Position Delta", &positionDelta[0]))
            changed = true;
        if (ImGui::DragFloat2("Walk Delta", &walkDelta[0]))
            changed = true;
    }
    return changed;
}

// Walk in a given direction on the view/right/up vectors
void CameraComponent::Walk(const glm::vec3& planes)
{
    walkDelta += viewDirection * planes.z;
    walkDelta += right * planes.x;
    walkDelta += up * planes.y;
}

// Walk towards the focal point
void CameraComponent::Dolly(float distance)
{
    positionDelta += viewDirection * distance;
}

// Orbit around the focal point, keeping y 'Up'
void CameraComponent::Orbit(const glm::vec2& angle)
{
    orbitDelta += angle;
}

bool CameraComponent::Moving()
{
    if (glm::length2(walkDelta) < (FLT_EPSILON * FLT_EPSILON) &&
        glm::length2(orbitDelta) < (FLT_EPSILON * FLT_EPSILON) &&
        glm::length2(positionDelta) < (FLT_EPSILON * FLT_EPSILON))
    {
        return false;
    }
    return true;
}
// The film/window size of the rendering
void CameraComponent::SetFilmSize(const glm::uvec2& size)
{
    filmSize = size;
    aspectRatio = size.x / float(size.y);
}

// Where camera is, what it is looking at.
void camera_set_lookat(Entity* entity, const glm::vec3& pos, const glm::vec3& point)
{
    if (entity)
    {
        assert(component_get<CameraComponent>(entity) &&
            component_get<BodyComponent>(entity));
        auto camera = component_get<CameraComponent>(entity);
        auto& position = component_get<BodyComponent>(entity)->position;
        auto& orientation = component_get<BodyComponent>(entity)->orientation;

        // From
        position = pos;

        // Focal
        camera->focalPoint = point;

        // TODO
        // Work out direction
        camera->viewDirection = camera->focalPoint - position;
        camera->viewDirection = glm::normalize(camera->viewDirection);

        // Get camera orientation relative to -z
        orientation = QuatFromVectors(glm::vec3(0.0f, 0.0f, -1.0f), camera->viewDirection);
        orientation = glm::normalize(orientation);

        update_right_up(entity);
    }
}

void camera_components_update(TimeDelta dt)
{
    auto rect = jorvik.spDevice->GetWindowSize();

    auto list = entities_with_components(CameraComponent::CID | BodyComponent::CID);
    for (auto entity : list)
    {
        if (entity)
        {
            auto camera = component_get<CameraComponent>(entity);
            auto position = component_get<BodyComponent>(entity)->position;
            auto orientation = component_get<BodyComponent>(entity)->orientation;

            // The half-width of the viewport, in world space
            camera->halfAngle = float(tan(glm::radians(camera->fieldOfView) / 2.0));

            bool changed = false;

            if (dt >= .001f)
            {
                if (camera->orbitDelta != glm::vec2(0.0f))
                {
                    const float settlingTimeMs = 80;
                    float frac = std::min(dt / settlingTimeMs * 1000.0f, 1.0f);

                    // Get a proportion of the remaining turn angle, based on the time delta
                    glm::vec2 angle = frac * camera->orbitDelta;

                    // Reduce the orbit delta remaining for next time
                    camera->orbitDelta *= (1.0f - frac);
                    if (glm::all(glm::lessThan(glm::abs(camera->orbitDelta), glm::vec2(.00001f))))
                    {
                        camera->orbitDelta = glm::vec2(0.0f);
                    }

                    // 2 rotations, about right and world up, for the camera
                    glm::quat rotY = glm::angleAxis(glm::radians(angle.y), glm::vec3(camera->right));
                    glm::quat rotX = glm::angleAxis(glm::radians(angle.x), glm::vec3(0.0f, 1.0f, 0.0f));

                    // Concatentation of the current rotations with the new one
                    orientation = orientation * rotY * rotX;
                    orientation = glm::normalize(orientation);

                    // Recalculate position from the new view direction, relative to the focal point
                    float distance = glm::length(camera->focalPoint - position);
                    camera->viewDirection = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f) * orientation);
                    position = camera->focalPoint - (camera->viewDirection * distance);

                    update_right_up(entity);
                    changed = true;
                }

                if (camera->positionDelta != glm::vec3(0.0f))
                {
                    const float settlingTimeMs = 50;
                    float frac = std::min(dt / settlingTimeMs * 1000, 1.0f);
                    glm::vec3 distance = frac * camera->positionDelta;
                    camera->positionDelta *= (1.0f - frac);

                    position += distance;
                    changed = true;
                }

                if (camera->walkDelta != glm::vec3(0.0f))
                {
                    const float settlingTimeMs = 50;
                    float frac = std::min(dt / settlingTimeMs * 1000, 1.0f);
                    glm::vec3 distance = frac * camera->walkDelta;
                    camera->walkDelta *= (1.0f - frac);

                    position += distance;
                    camera->focalPoint += distance;
                    changed = true;
                }
            }

            if (changed)
            {
                update_right_up(entity);
            }

            camera->SetFilmSize(glm::uvec2(rect.x, rect.y));
        }
    }
};


// Given a screen coordinate, return a ray leaving the camera and entering the world at that 'pixel'
Ray camera_get_world_ray(Entity* entity, const glm::vec2& imageSample)
{
    auto position = component_get<BodyComponent>(entity)->position;
    auto& camera = *component_get<CameraComponent>(entity);

    // Could move some of this maths out of here for speed, but this isn't time critical
    // TODO: Lens
    auto lensRand = glm::circularRand(0.0f);

    auto dir = camera.viewDirection;
    float x = ((imageSample.x * 2.0f) / camera.filmSize.x) - 1.0f;
    float y = ((imageSample.y * 2.0f) / camera.filmSize.y) - 1.0f;

    // Take the view direction and adjust it to point at the given sample, based on the 
    // the frustum 
    dir += (camera.right * (camera.halfAngle * camera.aspectRatio * x));
    dir -= (camera.up * (camera.halfAngle * y));
    //dir = normalize(dir);
    float ft = (glm::length(camera.focalPoint - position) - 1.0f) / glm::length(dir);
    glm::vec3 focasPoint = position + dir * ft;

    glm::vec3 lensPoint = position;
    lensPoint += (camera.right * lensRand.x);
    lensPoint += (camera.up * lensRand.y);
    dir = glm::normalize(focasPoint - lensPoint);

    return Ray{ lensPoint, dir };
}


glm::mat4 camera_get_lookat(Entity* entity)
{
    ASSERT_COMPONENT(entity, CameraComponent);
    ASSERT_COMPONENT(entity, BodyComponent);

    auto pCamera = component_get<CameraComponent>(entity);
    assert(pCamera);
    if (pCamera)
    {
        auto position = component_get<BodyComponent>(entity)->position;
        auto orientation = component_get<BodyComponent>(entity)->orientation;

        glm::vec3 up = glm::normalize(orientation * glm::vec3(0.0f, 1.0f, 0.0f));
        return glm::lookAtRH(position, pCamera->focalPoint, up);
    }
    return glm::mat4(1.0f);
}

// The current projection matrix
// This is different depending on the API
glm::mat4 camera_get_projection(Entity* entityId, ProjectionType type)
{
    ASSERT_COMPONENT(entityId, CameraComponent);

    auto pCamera = component_get<CameraComponent>(entityId);
    assert(pCamera);
    if (pCamera)
    {
        glm::mat4 projection;
        if (pCamera->mode == CameraMode::Perspective)
        {
            projection = glm::perspectiveFovRH(glm::radians(pCamera->fieldOfView), float(pCamera->filmSize.x), float(pCamera->filmSize.y), pCamera->depthRange.y, pCamera->depthRange.x);
        }
        else
        {
            float bottom = float(pCamera->filmSize.y);
            float top = float(0.0f);
            projection = glm::orthoRH(0.0f, float(pCamera->filmSize.x), bottom, top, 0.0f, 1.0f);
        }

        // Now using Right Hand coordinate system and viewport mapping 0->1 for all.
        /*if (type == ProjectionType::D3D)
        {
            // For D3D we need to fix clip space, since GL maps -1->1, but GL maps 0->1.
            // This is the way to do it.
            glm::mat4 scaleFix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, .5f));
            glm::mat4 translate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.5f));
            projection = translate * scaleFix * projection;
        }
        */
        return projection;
    }
    return glm::mat4(1.0f);
}

} // Mgfx

