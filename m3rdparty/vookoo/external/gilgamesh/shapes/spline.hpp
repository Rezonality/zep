////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// gilgamesh: Spline geometry class
// 

#ifndef MESHUTILS_spline_INCLUDED
#define MESHUTILS_spline_INCLUDED

// note that this class has low dependencies
#include <glm/glm.hpp>
#include <cmath>
#include <utility>

namespace gilgamesh {

enum class SplineType {
  /// Catmul-Rom splines interpolate through 
  CatmullRom,
  //Bezier,
};

/// An indexed spline of various kinds.
/// For a closed spline, you will need to replicate control points at the start and end using the indices.
class Spline {
public:
  Spline() {
  }

  Spline(const std::vector<glm::vec3> &controlPoints, const std::vector<uint32_t> &indices, SplineType splineType) : controlPoints_(controlPoints), indices_(indices) {
    switch (splineType) {
      case SplineType::CatmullRom: {
        // http://www.mvps.org/directx/articles/catmull/
        matrix_[0] = glm::vec4(0, -1, 2, -1);
        matrix_[1] = glm::vec4(2, 0, -5, 3);
        matrix_[2] = glm::vec4(0, 1, 4, -3);
        matrix_[3] = glm::vec4(0, 0, -1, 1);
      } break;
    }
  }

  // call this function to make a mesh
  // This works with the gilgamesh mesh, but is still generic.
  template <class Mesh>
  size_t build(Mesh &mesh, const glm::mat4 &transform = glm::mat4(), const glm::vec4 &color=glm::vec4(1)) {
    auto vertex = [&mesh, &transform, &color](const glm::vec3 &pos, const glm::vec3 &normal, const glm::vec2 &uv) {
      mesh.addVertexTransformed(transform, pos, normal, uv, color);
    };

    auto index = [&mesh](size_t idx) {
      mesh.addIndex(idx);
    };

    size_t firstIndex = mesh.vertices().size();

    buildMesh(vertex, index, firstIndex);
    
    // return the first index of the mesh
    return firstIndex;
  }

  // call this function to generate vertices and indices.
  template <class Vertex, class Index>
  void buildMesh(Vertex vertex, Index index, size_t firstIndex=0, int numVSegments=8) {
    if (controlPoints_.size() < 4) return;

    std::vector<glm::vec3> subdivPoints((controlPoints_.size()-2) * numYSegments + 1);
    int numSegs = int(controlPoints_.size());
    size_t numVertices = 0;
    glm::vec3 normal(0, 0, 1);
    for (int seg = 0; seg < numSegs-3; ++seg) {
      for (int vseg = 0; vseg != numVSegments; ++vseg) {
        float t = vseg * (1.0f/numVSegments);
        glm::vec2 uv(0, float(seg) + v);
        glm::vec3 pos = evaluate(seg, t);
        vertex(pos, uv, normal);
        numVertices++;
      }
    }
    vertex(controlPoints_[numSegs-1], glm::vec2(0, float(numSegs-3)), normal);
    numVertices++;

    for (size_t i = 0; i != numVertices; ++i) {
      index(firstIndex + i);
    }
  }

  /// Evaluate a point on the spline. 0 <= seg < numIndices()-3
  glm::vec3 evaluate(int seg, float t) {
    const glm::vec3 &p0 = controlPoints_[indices_[seg+0]];
    const glm::vec3 &p1 = controlPoints_[indices_[seg+1]];
    const glm::vec3 &p2 = controlPoints_[indices_[seg+2]];
    const glm::vec3 &p3 = controlPoints_[indices_[seg+3]];
    glm::vec4 tpower(1, t, t*t, t*t*t);
    glm::vec4 beta = tpower * matrix_;
    return beta.x * p0 + beta.y * p1 + beta.z * p2 + beta.w * p3;
  }

  /// Return the number of control points in the spline.
  size_t numControlPoints() const { return controlPoints_.size(); }
  size_t numIndices() const { return indices_.size(); }
private:
  float radius_;
  glm::mat4 matrix_;
  std::vector<glm::vec3> controlPoints_;
  std::vector<uint32_t> indices_;
};

} // gilgamesh

#endif
