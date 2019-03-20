#include "utils.h"

//#include "visual/camera.h"
#include "input.h"
#include "control.h"
//#include "visual/physics.h"
//#include "visual/mesh.h"
//#include "visual/body.h"
//#include "visual/material.h"

namespace Mgfx
{

// These IDs must never change!
//uint64_t BodyComponent::CID = (1 << 1);
uint64_t ControllableComponent::CID = (1 << 3);
//uint64_t CameraComponent::CID = (1 << 4);
//uint64_t MeshComponent::CID = (1 << 5);
//uint64_t MaterialComponent::CID = (1 << 7);

void components_register()
{
    //component_register<BodyComponent>();
    component_register<ControllableComponent>();
    //component_register<CameraComponent>();
    //component_register<MeshComponent>();
    //component_register<MaterialComponent>();
}


}
