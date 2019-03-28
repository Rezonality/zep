////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// gilgamesh: scene class: collection of meshes with a matrix heirachy
// 

#ifndef MESHUTILS_SCENE_INCLUDED
#define MESHUTILS_SCENE_INCLUDED

#include <gilgamesh/mesh.hpp>
#include <vector>
#include <memory>

// Scene class. Note that the scene does not own its components.
namespace gilgamesh {
  class scene {
  public:
    scene() {
    }

    // note that the scene does not own the meshes. You have to manage them yourself.
    const std::vector<mesh *> &meshes() const { return meshes_; }
    const std::vector<glm::mat4> &transforms() const { return transforms_; }
    const std::vector<int> &parent_transforms() const { return parent_transforms_; }
    const std::vector<int> &mesh_indices() const { return mesh_indices_; }

    int addMesh(mesh *mesh) {
      size_t result = meshes_.size();
      meshes_.emplace_back(mesh);
      return (int)result;
    }

    int addNode(const glm::mat4 &mat, int parent, int mesh) {
      size_t result = transforms_.size();
      transforms_.emplace_back(mat);
      parent_transforms_.emplace_back(parent);
      mesh_indices_.emplace_back(mesh);
      return (int)result;
    }

  private:
    std::vector<mesh *> meshes_;
    std::vector<glm::mat4> transforms_;
    std::vector<int> parent_transforms_;
    std::vector<int> mesh_indices_;
  };
}

#endif
