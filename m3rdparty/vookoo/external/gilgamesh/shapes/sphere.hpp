////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// gilgamesh: sphere geometry class
// 

#ifndef MESHUTILS_SPHERE_INCLUDED
#define MESHUTILS_SPHERE_INCLUDED

#include <glm/glm.hpp>

namespace gilgamesh {

// A sphere centred on the origin.
class sphere {
public:
  sphere(float radius=1) : radius_(radius) {
  }

  // call this function to make a mesh
  // This works with the gilgamesh mesh, but is still generic.
  template <class Mesh>
  size_t build(Mesh &mesh, const glm::mat4 &transform = glm::mat4(), const glm::vec4 &color=glm::vec4(1), int num_lattitude=10, bool gen_uvs=false) {
    //int vi = 0;
    auto vertex = [&](const glm::vec3 &pos, const glm::vec3 &normal, const glm::vec2 &uv) {
      mesh.addVertexTransformed(transform, pos, normal, uv, color);
      //printf("v%d\n", (int)vi++);
    };

    auto index = [&](size_t idx) {
      mesh.addIndex(idx);
      //printf("i%d\n", (int)idx);
    };

    size_t first_index = mesh.vertices().size();

    buildMesh(vertex, index, first_index, num_lattitude, gen_uvs);
    
    // return the first index of the mesh
    return first_index;
  }

  // call this function to generate vertices and indices.
  template <class Vertex, class Index>
  void buildMesh(Vertex vertex, Index index, size_t first_index, int num_lattitude, bool gen_uvs) {
    // generate vertices
    float length = 3.141592653589793f / (float)num_lattitude;
    int min_points = 1; //gen_uvs ? 6 : 1;
    int vi = 0;
    for (int i = 0; i <= num_lattitude; ++i) {
      //printf("i=%d\n", i);
      float phi = i * (3.141592653589793f / num_lattitude);
      float cosphi = std::cos(phi);
      float sinphi = std::sin(phi);
      int n = std::max(min_points, int(sinphi * (3.141592653589793f*2 / length)));
      int max_j = n + (gen_uvs ? 1 : 0);
      for (int j = 0; j < max_j; ++j) {
        float theta = j * (3.141592653589793f*2/n);
        float costheta = std::cos(theta);
        float sintheta = std::sin(theta);
        glm::vec3 pos(sinphi * costheta, cosphi, sinphi * sintheta);
        glm::vec2 uv(j * (1.0f/n), i * (1.0f / num_lattitude));
        vertex(pos * radius_, pos, uv);
      }
    }

    // generate indices
    float pphi = 0;
    int pn = min_points;
    size_t pidx = first_index;
    for (int i = 1; i <= num_lattitude; ++i) {
      //printf("i=%d\n", i);
      float phi = i * (3.141592653589793f / num_lattitude);
      float sinphi = std::sin(phi);
      int n = std::max(min_points, int(sinphi * (3.141592653589793f*2 / length)));
      size_t idx = pidx + pn + (gen_uvs ? 1 : 0);

      // idx is the first vertex on the current row, pidx is the first on the previous row.
      // Wrap at the meridian if not generating uv coordinates.
      // ie. there is one less point 
      auto wrap = [gen_uvs](int j, int n) {
        return j == n && !gen_uvs ? 0 : j;
      };

      for (int j = 0, pj = 0; j < n || pj < pn;) {
        //printf("%d/%d %d/%d idx=%d/%d\n", pj, pn, j, n, (int)pidx, (int)idx);
        size_t i0 = idx + wrap(j, n);
        size_t i1 = pidx + wrap(pj, pn);
        size_t i2 = 0;
        if (j * pn < pj * n) { // equivalent to j / n < pj / pn
          i2 = idx + wrap(j + 1, n);
          ++j;
        } else {
          i2 = pidx + wrap(pj + 1, pn);
          ++pj;
        }
        if (i1 != i2 && i0 != i2) {
          index(i0);
          index(i1);
          index(i2);
        }
      }
      pidx = idx; pphi = phi; pn = n;
    }
  }
private:
  float radius_;
};

} // gilgamesh

#endif
