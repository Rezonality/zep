////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// gilgamesh: cylinder geometry class
// 

#ifndef MESHUTILS_CYLINDER_INCLUDED
#define MESHUTILS_CYLINDER_INCLUDED

// note that this class has low dependencies
#include <glm/glm.hpp>
#include <cmath>

namespace gilgamesh {

// A cylinder centred on the origin with 'y' axis
class cylinder {
public:
  enum Feature {
    top = 1,
    bottom = 2,
    body = 4,
  };

  cylinder(float radius=1, float height=1) : radius_(radius), height_(height) {
  }

  // call this function to make a mesh
  // This works with the gilgamesh mesh, but is still generic.
  template <class Mesh>
  size_t build(Mesh &mesh, const glm::mat4 &transform = glm::mat4(), const glm::vec4 &color=glm::vec4(1), int num_y_segments=1, int num_radial_segments=16, Feature features=(Feature)(top|body|bottom)) {
    auto vertex = [&mesh, &transform, &color](const glm::vec3 &pos, const glm::vec3 &normal, const glm::vec2 &uv) {
      mesh.addVertexTransformed(transform, pos, normal, uv, color);
    };

    auto index = [&mesh](size_t idx) {
      mesh.addIndex(idx);
    };

    size_t first_index = mesh.vertices().size();

    buildMesh(vertex, index, first_index, num_y_segments, num_radial_segments, features);
    
    // return the first index of the mesh
    return first_index;
  }

  // call this function to generate vertices and indices.
  template <class Vertex, class Index>
  void buildMesh(Vertex vertex, Index index, size_t first_index=0, int num_y_segments=1, int num_radial_segments=16, Feature features=(Feature)(top|body|bottom)) {
    if (features & (int)bottom) {
      size_t centre_idx = first_index++;
      float y = height_ * (-0.5f);
      glm::vec3 normal(0, -1, 0);
      glm::vec2 uv(0, 0);
      vertex(glm::vec3(0, y, 0), normal, uv);

      for (int r = 0; r <= num_radial_segments; ++r) {
        float theta = r * (3.141592653589793f*2/num_radial_segments);
        float costheta = std::cos(theta);
        float sintheta = std::sin(theta);
        vertex(glm::vec3(costheta * radius_, y, sintheta * radius_), normal, uv);
        first_index++;
      }

      for (int r = 0; r < num_radial_segments; ++r) {
        index(centre_idx);
        index(centre_idx + 1 + r);
        index(centre_idx + 2 + r);
      }
    }

    if (features & (int)top) {
      size_t centre_idx = first_index++;
      float y = height_ * (0.5f);
      glm::vec3 normal(0, 1, 0);
      glm::vec2 uv(1, 1);
      vertex(glm::vec3(0, y, 0), normal, uv);

      for (int r = 0; r <= num_radial_segments; ++r) {
        float theta = r * (3.141592653589793f*2/num_radial_segments);
        float costheta = std::cos(theta);
        float sintheta = std::sin(theta);
        vertex(glm::vec3(costheta * radius_, y, sintheta * radius_), normal, uv);
        first_index++;
      }

      for (int r = 0; r < num_radial_segments; ++r) {
        index(centre_idx);
        index(centre_idx + 2 + r);
        index(centre_idx + 1 + r);
      }
    }

    if (features & (int)body) {
      for (int iy = 0; iy <= num_y_segments; ++iy) {
        float y = height_ * (-0.5f) + iy * (height_ / num_y_segments);
        for (int r = 0; r <= num_radial_segments; ++r) {
          float theta = r * (3.141592653589793f*2/num_radial_segments);
          float costheta = std::cos(theta);
          float sintheta = std::sin(theta);
          glm::vec3 normal(costheta, 0, sintheta);
          glm::vec2 uv(r * (1.0f/num_radial_segments), iy * (1.0f/num_y_segments));
          glm::vec3 pos(costheta * radius_, y, sintheta * radius_);
          vertex(pos, normal, uv);
        }
      }

      for (int iy = 0; iy < num_y_segments; ++iy) {
        for (int r = 0; r < num_radial_segments; ++r) {
          size_t idx0 = first_index + iy * (num_radial_segments+1);
          size_t idx1 = idx0 + (num_radial_segments+1);
          index(idx0 + r);
          index(idx1 + r);
          index(idx0 + r + 1);
          index(idx0 + r + 1);
          index(idx1 + r);
          index(idx1 + r + 1);
        }
      }
    }
  }
private:
  float radius_;
  float height_;
};

} // gilgamesh

#endif
